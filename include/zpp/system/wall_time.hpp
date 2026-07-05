#pragma once

#include <zpp/namespace.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <version>

#ifndef ZPP_WALL_TIME_FORCE_LEGACY
#define ZPP_WALL_TIME_FORCE_LEGACY 0
#endif

#ifndef ZPP_WALL_TIME_HAS_STD_FORMAT
#if !ZPP_WALL_TIME_FORCE_LEGACY && defined(__cpp_lib_format) &&                \
    __cpp_lib_format >= 201907L
#define ZPP_WALL_TIME_HAS_STD_FORMAT 1
#else
#define ZPP_WALL_TIME_HAS_STD_FORMAT 0
#endif
#endif

#ifndef ZPP_WALL_TIME_HAS_CHRONO_TIMEZONE
#if !ZPP_WALL_TIME_FORCE_LEGACY && defined(__cpp_lib_chrono) &&                \
    __cpp_lib_chrono >= 201907L
#define ZPP_WALL_TIME_HAS_CHRONO_TIMEZONE 1
#else
#define ZPP_WALL_TIME_HAS_CHRONO_TIMEZONE 0
#endif
#endif

#ifndef ZPP_WALL_TIME_USE_STD_CHRONO_FORMAT
#define ZPP_WALL_TIME_USE_STD_CHRONO_FORMAT                                    \
  (ZPP_WALL_TIME_HAS_STD_FORMAT && ZPP_WALL_TIME_HAS_CHRONO_TIMEZONE)
#endif

#if ZPP_WALL_TIME_HAS_STD_FORMAT
#include <format>
#endif

#if ZPP_WALL_TIME_USE_STD_CHRONO_FORMAT
#include <stdexcept>
#endif

NSB_ZPP

/**
 * @brief Wall-clock formatting utilities.
 *
 * `wall_time` is intentionally stateless. It converts
 * `std::chrono::system_clock::time_point` values to local time and keeps a
 * legacy localtime fallback for standard libraries without C++20 timezone or
 * format support.
 */
struct wall_time {
  /**
   * @brief Clock used for wall-clock timestamps.
   */
  using clock_type = std::chrono::system_clock;

  /**
   * @brief Time point type accepted by the formatting helpers.
   */
  using time_point = clock_type::time_point;

  /**
   * @brief Return current Unix epoch milliseconds.
   *
   * @return Milliseconds since the Unix epoch.
   */
  static std::int64_t epoch_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               clock_type::now().time_since_epoch())
        .count();
  }

  /**
   * @brief Return current Unix epoch microseconds.
   *
   * @return Microseconds since the Unix epoch.
   */
  static std::int64_t epoch_us() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               clock_type::now().time_since_epoch())
        .count();
  }

  /**
   * @brief Return current Unix epoch nanoseconds.
   *
   * @return Nanoseconds since the Unix epoch.
   */
  static std::int64_t epoch_ns() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               clock_type::now().time_since_epoch())
        .count();
  }

  /**
   * @brief Format a wall-clock time point as local time.
   *
   * The output format is `yyyy-mm-dd hh:mm:ss.uuuuuu`.
   *
   * @param tp Wall-clock time point to format.
   * @param out Output string reused by the caller.
   * @return Reference to `out`.
   */
  static std::string &to_str(time_point tp, std::string &out) {
    const auto microsecond_tp =
        std::chrono::floor<std::chrono::microseconds>(tp);
#if ZPP_WALL_TIME_USE_STD_CHRONO_FORMAT
    try {
      const auto local_time =
          std::chrono::zoned_time{std::chrono::current_zone(), microsecond_tp};
      out = std::format("{:%Y-%m-%d %H:%M:%S}", local_time);
      return out;
    } catch (const std::runtime_error &) {
      return to_str_legacy(microsecond_tp, out);
    }
#else
    return to_str_legacy(microsecond_tp, out);
#endif
  }

  /**
   * @brief Format the current wall-clock time as local time.
   *
   * The output format is `yyyy-mm-dd hh:mm:ss.uuuuuu`.
   *
   * @param out Output string reused by the caller.
   * @return Reference to `out`.
   */
  static std::string &now_str(std::string &out) {
    return to_str(clock_type::now(), out);
  }

private:
  static std::string &to_str_legacy(
      std::chrono::time_point<clock_type, std::chrono::microseconds> tp,
      std::string &out) {
    const auto seconds_tp = std::chrono::floor<std::chrono::seconds>(tp);
    const auto microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(
            tp.time_since_epoch() - seconds_tp.time_since_epoch())
            .count();
    const std::time_t seconds_since_epoch = clock_type::to_time_t(seconds_tp);

    std::tm local_tm{};
    if (!to_local_tm(seconds_since_epoch, local_tm)) {
      out.clear();
      return out;
    }

    char buffer[27]{};
    const int written = std::snprintf(
        buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%06lld",
        local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
        local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
        static_cast<long long>(microseconds));
    if (written < 0) {
      out.clear();
      return out;
    }

    out.assign(buffer, static_cast<std::size_t>(written));
    return out;
  }

  static bool to_local_tm(std::time_t seconds_since_epoch,
                          std::tm &out) noexcept {
#if defined(ZSYS_WINDOWS) || defined(_WIN32)
    return localtime_s(&out, &seconds_since_epoch) == 0;
#elif defined(__GNUC__)
    return localtime_r(&seconds_since_epoch, &out) != nullptr;
#else
    const std::tm *local_tm = std::localtime(&seconds_since_epoch);
    if (local_tm == nullptr) {
      return false;
    }

    out = *local_tm;
    return true;
#endif
  }
};

NSE_ZPP
