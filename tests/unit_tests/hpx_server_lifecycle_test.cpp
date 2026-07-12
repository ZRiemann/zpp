#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <zpp/hpx/server_lifecycle.hpp>

namespace {

struct lifecycle_state {
  z::err_t configure_result{z::ERR_OK};
  z::err_t run_result{z::ERR_OK};
  z::err_t stop_result{z::ERR_OK};
  z::err_t wait_stop_result{z::ERR_OK};
  std::vector<std::string> events;
};

lifecycle_state &state() {
  static lifecycle_state value;
  return value;
}

void reset_state() { state() = lifecycle_state{}; }

class fake_server {
public:
  fake_server(int argc, char **argv) : _argc(argc), _argv(argv) {
    state().events.emplace_back("construct");
  }

  ~fake_server() { state().events.emplace_back("destruct"); }

  z::err_t configure() {
    state().events.emplace_back("configure");
    return state().configure_result;
  }

  z::err_t run() {
    state().events.emplace_back("run");
    return state().run_result;
  }

  void loop() { state().events.emplace_back("loop"); }

  z::err_t stop() {
    state().events.emplace_back("stop");
    return state().stop_result;
  }

  z::err_t wait_stop() {
    state().events.emplace_back("wait_stop");
    return state().wait_stop_result;
  }

private:
  int _argc;
  char **_argv;
};

std::vector<std::string> events() { return state().events; }

} // namespace

TEST(HpxServerLifecycleTest, ConfigureFailureSkipsStartupAndCleanup) {
  reset_state();
  state().configure_result = z::ERR_FAIL;
  char app[] = "test";
  char *argv[] = {app};

  EXPECT_EQ(z::ERR_FAIL, z::hpx::run_server_lifecycle<fake_server>(1, argv));

  const std::vector<std::string> expected{"construct", "configure", "destruct"};
  EXPECT_EQ(expected, events());
}

TEST(HpxServerLifecycleTest, RunFailureSkipsLoopAndCleanup) {
  reset_state();
  state().run_result = z::ERR_TIMEOUT;
  char app[] = "test";
  char *argv[] = {app};

  EXPECT_EQ(z::ERR_TIMEOUT,
            z::hpx::run_server_lifecycle<fake_server>(1, argv));

  const std::vector<std::string> expected{"construct", "configure", "run",
                                          "destruct"};
  EXPECT_EQ(expected, events());
}

TEST(HpxServerLifecycleTest, StopFailureStillWaitsAndReturnsFirstCleanupError) {
  reset_state();
  state().stop_result = z::ERR_FAIL;
  state().wait_stop_result = z::ERR_TIMEOUT;
  char app[] = "test";
  char *argv[] = {app};

  EXPECT_EQ(z::ERR_FAIL, z::hpx::run_server_lifecycle<fake_server>(1, argv));

  const std::vector<std::string> expected{
      "construct", "configure", "run", "loop", "stop", "wait_stop", "destruct"};
  EXPECT_EQ(expected, events());
}

TEST(HpxServerLifecycleTest, WaitStopFailureIsReturnedWhenStopSucceeds) {
  reset_state();
  state().wait_stop_result = z::ERR_TIMEOUT;
  char app[] = "test";
  char *argv[] = {app};

  EXPECT_EQ(z::ERR_TIMEOUT,
            z::hpx::run_server_lifecycle<fake_server>(1, argv));

  const std::vector<std::string> expected{
      "construct", "configure", "run", "loop", "stop", "wait_stop", "destruct"};
  EXPECT_EQ(expected, events());
}

TEST(HpxServerLifecycleTest, SuccessfulLifecycleReturnsOk) {
  reset_state();
  char app[] = "test";
  char *argv[] = {app};

  EXPECT_EQ(z::ERR_OK, z::hpx::run_server_lifecycle<fake_server>(1, argv));

  const std::vector<std::string> expected{
      "construct", "configure", "run", "loop", "stop", "wait_stop", "destruct"};
  EXPECT_EQ(expected, events());
}
