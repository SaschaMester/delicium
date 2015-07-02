# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from catapult_base import cloud_storage
from telemetry import decorators
from telemetry.internal.results import results_options
from telemetry.internal import story_runner
from telemetry.internal.util import command_line
from telemetry.internal.util import exception_formatter
from telemetry import page
from telemetry.page import page_test
from telemetry.page import test_expectations
from telemetry.web_perf import timeline_based_measurement

Disabled = decorators.Disabled
Enabled = decorators.Enabled


class InvalidOptionsError(Exception):
  """Raised for invalid benchmark options."""
  pass


class BenchmarkMetadata(object):
  def __init__(self, name, description='', rerun_options=None):
    self._name = name
    self._description = description
    self._rerun_options = rerun_options

  @property
  def name(self):
    return self._name

  @property
  def description(self):
    return self._description

  @property
  def rerun_options(self):
    return self._rerun_options


class Benchmark(command_line.Command):
  """Base class for a Telemetry benchmark.

  A benchmark packages a measurement and a PageSet together.
  Benchmarks default to using TBM unless you override the value of
  Benchmark.test, or override the CreatePageTest method.

  New benchmarks should override CreateStorySet.
  """
  options = {}
  test = timeline_based_measurement.TimelineBasedMeasurement

  def __init__(self, max_failures=None):
    """Creates a new Benchmark.

    Args:
      max_failures: The number of story run's failures before bailing
          from executing subsequent page runs. If None, we never bail.
    """
    self._max_failures = max_failures
    self._has_original_tbm_options = (
        self.CreateTimelineBasedMeasurementOptions.__func__ ==
        Benchmark.CreateTimelineBasedMeasurementOptions.__func__)
    has_original_create_page_test = (
        self.CreatePageTest.__func__ == Benchmark.CreatePageTest.__func__)
    assert self._has_original_tbm_options or has_original_create_page_test, (
        'Cannot override both CreatePageTest and '
        'CreateTimelineBasedMeasurementOptions.')

  @classmethod
  def Name(cls):
    return '%s.%s' % (cls.__module__.split('.')[-1], cls.__name__)

  @classmethod
  def AddCommandLineArgs(cls, parser):
    group = optparse.OptionGroup(parser, '%s test options' % cls.Name())
    if hasattr(cls, 'AddBenchmarkCommandLineArgs'):
      cls.AddBenchmarkCommandLineArgs(group)

    if cls.HasTraceRerunDebugOption():
      group.add_option(
          '--rerun-with-debug-trace',
          action='store_true',
          help='Rerun option that enables more extensive tracing.')

    if group.option_list:
      parser.add_option_group(group)

  @classmethod
  def HasTraceRerunDebugOption(cls):
    if hasattr(cls, 'HasBenchmarkTraceRerunDebugOption'):
      if cls.HasBenchmarkTraceRerunDebugOption():
        return True
    return False

  def GetTraceRerunCommands(self):
    if self.HasTraceRerunDebugOption():
      return [['Debug Trace', '--rerun-with-debug-trace']]
    return []

  def SetupTraceRerunOptions(self, browser_options, tbm_options):
    if self.HasTraceRerunDebugOption():
      if browser_options.rerun_with_debug_trace:
        self.SetupBenchmarkDebugTraceRerunOptions(tbm_options)
      else:
        self.SetupBenchmarkDefaultTraceRerunOptions(tbm_options)

  def SetupBenchmarkDefaultTraceRerunOptions(self, tbm_options):
    """Setup tracing categories associated with default trace option."""

  def SetupBenchmarkDebugTraceRerunOptions(self, tbm_options):
    """Setup tracing categories associated with debug trace option."""

  @classmethod
  def SetArgumentDefaults(cls, parser):
    default_values = parser.get_default_values()
    invalid_options = [
        o for o in cls.options if not hasattr(default_values, o)]
    if invalid_options:
      raise InvalidOptionsError('Invalid benchmark options: %s',
                                ', '.join(invalid_options))
    parser.set_defaults(**cls.options)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    pass

  # pylint: disable=unused-argument
  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    """Returns whether |value| can be added to the test results.

    Override this method to customize the logic of adding values to test
    results.

    Args:
      value: a value.Value instance (except failure.FailureValue,
        skip.SkipValue or trace.TraceValue which will always be added).
      is_first_result: True if |value| is the first result for its
          corresponding story.

    Returns:
      True if |value| should be added to the test results.
      Otherwise, it returns False.
    """
    return True

  def CustomizeBrowserOptions(self, options):
    """Add browser options that are required by this benchmark."""

  def GetMetadata(self):
    return BenchmarkMetadata(
        self.Name(), self.__doc__, self.GetTraceRerunCommands())

  def Run(self, finder_options):
    """Run this test with the given options.

    Returns:
      The number of failure values (up to 254) or 255 if there is an uncaught
      exception.
    """
    self.CustomizeBrowserOptions(finder_options.browser_options)

    pt = self.CreatePageTest(finder_options)
    pt.__name__ = self.__class__.__name__

    if hasattr(self, '_disabled_strings'):
      # pylint: disable=protected-access
      pt._disabled_strings = self._disabled_strings
    if hasattr(self, '_enabled_strings'):
      # pylint: disable=protected-access
      pt._enabled_strings = self._enabled_strings

    expectations = self.CreateExpectations()
    stories = self.CreateStorySet(finder_options)
    if isinstance(pt, page_test.PageTest):
      if any(not isinstance(p, page.Page) for p in stories.stories):
        raise Exception(
            'PageTest must be used with StorySet containing only '
            'telemetry.page.Page stories.')

    benchmark_metadata = self.GetMetadata()
    with results_options.CreateResults(
        benchmark_metadata, finder_options,
        self.ValueCanBeAddedPredicate) as results:
      try:
        story_runner.Run(pt, stories, expectations, finder_options, results,
                              max_failures=self._max_failures)
        return_code = min(254, len(results.failures))
      except Exception:
        exception_formatter.PrintFormattedException()
        return_code = 255

      bucket = cloud_storage.BUCKET_ALIASES[finder_options.upload_bucket]
      if finder_options.upload_results:
        results.UploadTraceFilesToCloud(bucket)
        results.UploadProfilingFilesToCloud(bucket)

      results.PrintSummary()
    return return_code

  def CreateTimelineBasedMeasurementOptions(self):
    """Return the TimelineBasedMeasurementOptions for this Benchmark.

    Override this method to configure a TimelineBasedMeasurement benchmark.
    Otherwise, override CreatePageTest for PageTest tests. Do not override
    both methods.
    """
    return timeline_based_measurement.Options()

  def CreatePageTest(self, options):  # pylint: disable=unused-argument
    """Return the PageTest for this Benchmark.

    Override this method for PageTest tests.
    Override, override CreateTimelineBasedMeasurementOptions to configure
    TimelineBasedMeasurement tests. Do not override both methods.

    Args:
      options: a browser_options.BrowserFinderOptions instance
    Returns:
      |test()| if |test| is a PageTest class.
      Otherwise, a TimelineBasedMeasurement instance.
    """
    is_page_test = issubclass(self.test, page_test.PageTest)
    is_tbm = self.test == timeline_based_measurement.TimelineBasedMeasurement
    if not is_page_test and not is_tbm:
      raise TypeError('"%s" is not a PageTest or a TimelineBasedMeasurement.' %
                      self.test.__name__)
    if is_page_test:
      assert self._has_original_tbm_options, (
          'Cannot override CreateTimelineBasedMeasurementOptions '
          'with a PageTest.')
      return self.test()  # pylint: disable=no-value-for-parameter

    opts = self.CreateTimelineBasedMeasurementOptions()
    self.SetupTraceRerunOptions(options, opts)
    return timeline_based_measurement.TimelineBasedMeasurement(opts)

  def CreateStorySet(self, options):
    """Creates the instance of StorySet used to run the benchmark.

    Can be overridden by subclasses.
    """
    del options  # unused
    # TODO(aiolos, nednguyen, eakufner): replace class attribute page_set with
    # story_set.
    if not hasattr(self, 'page_set'):
      raise NotImplementedError('This test has no "page_set" attribute.')
    return self.page_set()

  @classmethod
  def CreateExpectations(cls):
    """Get the expectations this test will run with.

    By default, it will create an empty expectations set. Override to generate
    custom expectations.
    """
    return test_expectations.TestExpectations()


def AddCommandLineArgs(parser):
  story_runner.AddCommandLineArgs(parser)


def ProcessCommandLineArgs(parser, args):
  story_runner.ProcessCommandLineArgs(parser, args)
