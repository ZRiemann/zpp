/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <glog/logging.h>

#include <folly/Portability.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <folly/portability/GTest.h>
#include <folly/executors/GlobalExecutor.h>
#include <zpp/core/server.h>
#if 0
// Define gflags for controlling folly global executors
// In main function of test executable
// FLAGS_folly_global_cpu_executor_threads = 2;
// will be overridden by command line args parsing by folly::Init
#include <gflags/gflags.h>

DEFINE_uint32(folly_global_cpu_executor_threads, 0,
              "Number of threads global CPUThreadPoolExecutor will create");

DEFINE_uint32(folly_global_io_executor_threads, 0,
              "Number of threads global IOThreadPoolExecutor will create");
#endif
/**
 * This is the recommended main function for all tests.
 * Before running all the tests, it initializes GoogleTest and folly.
 * By default, this configures Google Logging (glog) to output to stderr.
 *
 * The Makefile links it into all of the test programs so that tests do not need
 * to - and indeed should typically not - define their own main() functions
 * @file
 */
FOLLY_ATTR_WEAK int main(int argc, char** argv);

int main(int argc, char** argv) {
  // Enable glog logging to stderr by default.
  FLAGS_logtostderr = true;

  ::testing::InitGoogleTest(&argc, argv);

  FLAGS_folly_global_cpu_executor_threads = 2; // cover by arg parsing
  FLAGS_folly_global_io_executor_threads = 0;
  folly::Init init(&argc, &argv);
  // FLAGS_folly_global_cpu_executor_threads = 2; // cover the arg parsing
  z::server svr;
  return RUN_ALL_TESTS();
}
