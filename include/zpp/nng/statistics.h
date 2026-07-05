#pragma once

#include "defs.h"

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

NSB_NNG

class stat_view;

/// Returns a stable display name for an NNG statistic type.
inline std::string_view stat_type_name(int type) noexcept {
  switch (type) {
  case NNG_STAT_SCOPE:
    return "scope";
  case NNG_STAT_LEVEL:
    return "level";
  case NNG_STAT_COUNTER:
    return "counter";
  case NNG_STAT_STRING:
    return "string";
  case NNG_STAT_BOOLEAN:
    return "boolean";
  case NNG_STAT_ID:
    return "id";
  default:
    return "unknown";
  }
}

/// Returns a stable display name for an NNG statistic unit.
inline std::string_view stat_unit_name(int unit) noexcept {
  switch (unit) {
  case NNG_UNIT_NONE:
    return "none";
  case NNG_UNIT_BYTES:
    return "bytes";
  case NNG_UNIT_MESSAGES:
    return "messages";
  case NNG_UNIT_MILLIS:
    return "millis";
  case NNG_UNIT_EVENTS:
    return "events";
  default:
    return "unknown";
  }
}

/// Non-owning view over one node in an NNG statistics snapshot.
class stat_view {
public:
  /// Constructs an empty statistic view.
  stat_view() noexcept = default;

  /// Wraps an existing NNG statistic node without taking ownership.
  explicit stat_view(const nng_stat *stat) noexcept : stat_(stat) {}

  /// Returns true when this view refers to a statistic node.
  bool valid() const noexcept { return stat_ != nullptr; }

  /// Returns true when this view refers to a statistic node.
  explicit operator bool() const noexcept { return valid(); }

  /// Returns the wrapped NNG statistic node.
  const nng_stat *get() const noexcept { return stat_; }

  /// Returns the statistic name, or an empty string for an empty view.
  const char *name() const noexcept {
    if (stat_ == nullptr) {
      return "";
    }
    const auto *text = nng_stat_name(mutable_stat());
    return text == nullptr ? "" : text;
  }

  /// Returns the statistic description, or an empty string for an empty view.
  const char *desc() const noexcept {
    if (stat_ == nullptr) {
      return "";
    }
    const auto *text = nng_stat_desc(mutable_stat());
    return text == nullptr ? "" : text;
  }

  /// Returns the NNG statistic type, or -1 for an empty view.
  int type() const noexcept {
    return stat_ == nullptr ? -1 : nng_stat_type(mutable_stat());
  }

  /// Returns the NNG statistic unit, or NNG_UNIT_NONE for an empty view.
  int unit() const noexcept {
    return stat_ == nullptr ? NNG_UNIT_NONE : nng_stat_unit(mutable_stat());
  }

  /// Returns the capture timestamp in NNG clock milliseconds.
  std::uint64_t timestamp_ms() const noexcept {
    return stat_ == nullptr ? 0 : nng_stat_timestamp(mutable_stat());
  }

  /// Returns the numeric value for counter, level, and id statistics.
  std::uint64_t value() const noexcept {
    return stat_ == nullptr ? 0 : nng_stat_value(mutable_stat());
  }

  /// Returns the borrowed string value, or nullptr when the type is not string.
  const char *string_value() const noexcept {
    return stat_ == nullptr ? nullptr : nng_stat_string(mutable_stat());
  }

  /// Returns the boolean value for boolean statistics.
  bool bool_value() const noexcept {
    return stat_ != nullptr && nng_stat_bool(mutable_stat());
  }

  /// Returns the first child statistic.
  stat_view child() const noexcept {
    return stat_ == nullptr ? stat_view{}
                            : stat_view{nng_stat_child(mutable_stat())};
  }

  /// Returns the next sibling statistic.
  stat_view next() const noexcept {
    return stat_ == nullptr ? stat_view{}
                            : stat_view{nng_stat_next(mutable_stat())};
  }

  /// Returns an empty view on NNG versions that do not expose nng_stat_parent.
  stat_view parent() const noexcept { return {}; }

  /// Finds the first matching statistic by name within this subtree.
  stat_view find(const char *statistic_name) const noexcept {
    if (stat_ == nullptr || statistic_name == nullptr) {
      return {};
    }
    return stat_view{nng_stat_find(mutable_stat(), statistic_name)};
  }

  /// Finds a direct child statistic by name.
  stat_view find_child(const char *statistic_name) const noexcept {
    if (statistic_name == nullptr) {
      return {};
    }
    for (auto item = child(); item.valid(); item = item.next()) {
      if (std::string_view{item.name()} == statistic_name) {
        return item;
      }
    }
    return {};
  }

  /// Finds the statistics scope for a socket within this subtree.
  stat_view find_socket(nng_socket socket) const noexcept {
    return stat_ == nullptr
               ? stat_view{}
               : stat_view{nng_stat_find_socket(mutable_stat(), socket)};
  }

  /// Finds the statistics scope for a listener within this subtree.
  stat_view find_listener(nng_listener listener) const noexcept {
    return stat_ == nullptr
               ? stat_view{}
               : stat_view{nng_stat_find_listener(mutable_stat(), listener)};
  }

  /// Finds the statistics scope for a dialer within this subtree.
  stat_view find_dialer(nng_dialer dialer) const noexcept {
    return stat_ == nullptr
               ? stat_view{}
               : stat_view{nng_stat_find_dialer(mutable_stat(), dialer)};
  }

private:
  nng_stat *mutable_stat() const noexcept {
    return const_cast<nng_stat *>(stat_);
  }

  const nng_stat *stat_{nullptr};
};

/// RAII owner for an NNG statistics snapshot.
class stats_snapshot {
public:
  /// Constructs an empty snapshot.
  stats_snapshot() noexcept = default;

  /// Takes ownership of a snapshot root returned by nng_stats_get.
  explicit stats_snapshot(nng_stat *root) noexcept : root_(root) {}

  /// Releases the owned snapshot.
  ~stats_snapshot() noexcept { reset(); }

  stats_snapshot(const stats_snapshot &) = delete;
  stats_snapshot &operator=(const stats_snapshot &) = delete;

  /// Moves a snapshot owner.
  stats_snapshot(stats_snapshot &&other) noexcept
      : root_(std::exchange(other.root_, nullptr)) {}

  /// Moves a snapshot owner.
  stats_snapshot &operator=(stats_snapshot &&other) noexcept {
    if (this != &other) {
      reset();
      root_ = std::exchange(other.root_, nullptr);
    }
    return *this;
  }

  /// Returns true when this object owns a valid snapshot.
  bool valid() const noexcept { return root_ != nullptr; }

  /// Returns true when this object owns a valid snapshot.
  explicit operator bool() const noexcept { return valid(); }

  /// Returns the owned snapshot root.
  nng_stat *get() const noexcept { return root_; }

  /// Returns a view over the owned snapshot root.
  stat_view root() const noexcept { return stat_view{root_}; }

  /// Releases the current snapshot and optionally takes ownership of another.
  void reset(nng_stat *root = nullptr) noexcept {
    if (root_ != nullptr) {
      nng_stats_free(root_);
    }
    root_ = root;
  }

  /// Refreshes the snapshot, keeping the previous snapshot if collection fails.
  int refresh() noexcept {
    nng_stat *next_root = nullptr;
    const auto ret = nng_stats_get(&next_root);
    if (ret == 0) {
      reset(next_root);
    }
    return ret;
  }

  /// Finds the statistics scope for a socket within this snapshot.
  stat_view find_socket(nng_socket socket) const noexcept {
    return root().find_socket(socket);
  }

  /// Finds the statistics scope for a listener within this snapshot.
  stat_view find_listener(nng_listener listener) const noexcept {
    return root().find_listener(listener);
  }

  /// Finds the statistics scope for a dialer within this snapshot.
  stat_view find_dialer(nng_dialer dialer) const noexcept {
    return root().find_dialer(dialer);
  }

private:
  nng_stat *root_{nullptr};
};

/// Copied statistic record that remains valid after the snapshot is freed.
struct stat_record {
  /// Full diagnostic path for the statistic.
  std::string path;
  /// Statistic name.
  std::string name;
  /// Statistic description.
  std::string desc;
  /// NNG statistic type.
  int type{-1};
  /// NNG statistic unit.
  int unit{NNG_UNIT_NONE};
  /// Depth in the flattened tree.
  std::size_t depth{0};
  /// Capture timestamp in NNG clock milliseconds.
  std::uint64_t timestamp_ms{0};
  /// Numeric value for counter, level, and id statistics.
  std::optional<std::uint64_t> numeric_value;
  /// Boolean value for boolean statistics.
  std::optional<bool> bool_value;
  /// Copied string value for string statistics.
  std::string string_value;
};

/// Best-effort summary of common socket statistics.
struct socket_stats_summary {
  /// Socket identifier.
  std::optional<std::uint64_t> id;
  /// Socket name.
  std::string name;
  /// Socket protocol name.
  std::string protocol;
  /// Open dialer count.
  std::optional<std::uint64_t> dialers;
  /// Open listener count.
  std::optional<std::uint64_t> listeners;
  /// Open pipe count.
  std::optional<std::uint64_t> pipes;
  /// Rejected pipe count.
  std::optional<std::uint64_t> reject;
  /// Sent message count.
  std::optional<std::uint64_t> tx_msgs;
  /// Received message count.
  std::optional<std::uint64_t> rx_msgs;
  /// Sent byte count.
  std::optional<std::uint64_t> tx_bytes;
  /// Received byte count.
  std::optional<std::uint64_t> rx_bytes;
};

/// Best-effort summary of common listener and dialer statistics.
struct endpoint_stats_summary {
  /// Listener or dialer identifier.
  std::optional<std::uint64_t> id;
  /// Owning socket identifier.
  std::optional<std::uint64_t> socket_id;
  /// Endpoint URL.
  std::string url;
  /// Open pipe count.
  std::optional<std::uint64_t> pipes;
  /// Established connection or accepted connection count.
  std::optional<std::uint64_t> connect_or_accept;
  /// Refused connection count.
  std::optional<std::uint64_t> refused;
  /// Remote disconnect count.
  std::optional<std::uint64_t> disconnect;
  /// Canceled operation count.
  std::optional<std::uint64_t> canceled;
  /// Timeout error count.
  std::optional<std::uint64_t> timeout;
  /// Protocol error count.
  std::optional<std::uint64_t> proto;
  /// Authentication error count.
  std::optional<std::uint64_t> auth;
  /// Allocation failure count.
  std::optional<std::uint64_t> oom;
  /// Rejected pipe count.
  std::optional<std::uint64_t> reject;
  /// Other error count.
  std::optional<std::uint64_t> other;
};

/// Best-effort summary of common pipe statistics.
struct pipe_stats_summary {
  /// Pipe identifier.
  std::optional<std::uint64_t> id;
  /// Owning socket identifier.
  std::optional<std::uint64_t> socket_id;
  /// Sent message count.
  std::optional<std::uint64_t> tx_msgs;
  /// Received message count.
  std::optional<std::uint64_t> rx_msgs;
  /// Sent byte count.
  std::optional<std::uint64_t> tx_bytes;
  /// Received byte count.
  std::optional<std::uint64_t> rx_bytes;
};

/// Returns true when the statistic type exposes a numeric value.
inline bool stat_has_numeric_value(int type) noexcept {
  return type == NNG_STAT_COUNTER || type == NNG_STAT_LEVEL ||
         type == NNG_STAT_ID;
}

/// Returns a numeric direct child value when present.
inline std::optional<std::uint64_t>
stat_numeric_child(stat_view scope, const char *name) noexcept {
  const auto child = scope.find_child(name);
  if (!child.valid() || !stat_has_numeric_value(child.type())) {
    return std::nullopt;
  }
  return child.value();
}

/// Returns a copied string direct child value when present.
inline std::string stat_string_child(stat_view scope, const char *name) {
  const auto child = scope.find_child(name);
  if (!child.valid() || child.type() != NNG_STAT_STRING) {
    return {};
  }
  const auto *value = child.string_value();
  return value == nullptr ? std::string{} : std::string{value};
}

/// Builds a best-effort socket statistics summary from a socket scope.
inline socket_stats_summary summarize_socket(stat_view socket_scope) {
  socket_stats_summary summary;
  summary.id = stat_numeric_child(socket_scope, "id");
  summary.name = stat_string_child(socket_scope, "name");
  summary.protocol = stat_string_child(socket_scope, "protocol");
  summary.dialers = stat_numeric_child(socket_scope, "dialers");
  summary.listeners = stat_numeric_child(socket_scope, "listeners");
  summary.pipes = stat_numeric_child(socket_scope, "pipes");
  summary.reject = stat_numeric_child(socket_scope, "reject");
  summary.tx_msgs = stat_numeric_child(socket_scope, "tx_msgs");
  summary.rx_msgs = stat_numeric_child(socket_scope, "rx_msgs");
  summary.tx_bytes = stat_numeric_child(socket_scope, "tx_bytes");
  summary.rx_bytes = stat_numeric_child(socket_scope, "rx_bytes");
  return summary;
}

/// Builds a best-effort socket statistics summary from a snapshot and socket.
inline socket_stats_summary summarize_socket(const stats_snapshot &snapshot,
                                             nng_socket socket) {
  return summarize_socket(snapshot.find_socket(socket));
}

/// Builds a best-effort listener statistics summary from a listener scope.
inline endpoint_stats_summary summarize_listener(stat_view listener_scope) {
  endpoint_stats_summary summary;
  summary.id = stat_numeric_child(listener_scope, "id");
  summary.socket_id = stat_numeric_child(listener_scope, "socket");
  summary.url = stat_string_child(listener_scope, "url");
  summary.pipes = stat_numeric_child(listener_scope, "pipes");
  summary.connect_or_accept = stat_numeric_child(listener_scope, "accept");
  summary.disconnect = stat_numeric_child(listener_scope, "disconnect");
  summary.canceled = stat_numeric_child(listener_scope, "canceled");
  summary.timeout = stat_numeric_child(listener_scope, "timeout");
  summary.proto = stat_numeric_child(listener_scope, "proto");
  summary.auth = stat_numeric_child(listener_scope, "auth");
  summary.oom = stat_numeric_child(listener_scope, "oom");
  summary.reject = stat_numeric_child(listener_scope, "reject");
  summary.other = stat_numeric_child(listener_scope, "other");
  return summary;
}

/// Builds a best-effort listener statistics summary from a snapshot and
/// listener.
inline endpoint_stats_summary summarize_listener(const stats_snapshot &snapshot,
                                                 nng_listener listener) {
  return summarize_listener(snapshot.find_listener(listener));
}

/// Builds a best-effort dialer statistics summary from a dialer scope.
inline endpoint_stats_summary summarize_dialer(stat_view dialer_scope) {
  endpoint_stats_summary summary;
  summary.id = stat_numeric_child(dialer_scope, "id");
  summary.socket_id = stat_numeric_child(dialer_scope, "socket");
  summary.url = stat_string_child(dialer_scope, "url");
  summary.pipes = stat_numeric_child(dialer_scope, "pipes");
  summary.connect_or_accept = stat_numeric_child(dialer_scope, "connect");
  summary.refused = stat_numeric_child(dialer_scope, "refused");
  summary.disconnect = stat_numeric_child(dialer_scope, "disconnect");
  summary.canceled = stat_numeric_child(dialer_scope, "canceled");
  summary.timeout = stat_numeric_child(dialer_scope, "timeout");
  summary.proto = stat_numeric_child(dialer_scope, "proto");
  summary.auth = stat_numeric_child(dialer_scope, "auth");
  summary.oom = stat_numeric_child(dialer_scope, "oom");
  summary.reject = stat_numeric_child(dialer_scope, "reject");
  summary.other = stat_numeric_child(dialer_scope, "other");
  return summary;
}

/// Builds a best-effort dialer statistics summary from a snapshot and dialer.
inline endpoint_stats_summary summarize_dialer(const stats_snapshot &snapshot,
                                               nng_dialer dialer) {
  return summarize_dialer(snapshot.find_dialer(dialer));
}

/// Builds a best-effort pipe statistics summary from a pipe scope.
inline pipe_stats_summary summarize_pipe(stat_view pipe_scope) {
  pipe_stats_summary summary;
  summary.id = stat_numeric_child(pipe_scope, "id");
  summary.socket_id = stat_numeric_child(pipe_scope, "socket");
  summary.tx_msgs = stat_numeric_child(pipe_scope, "tx_msgs");
  summary.rx_msgs = stat_numeric_child(pipe_scope, "rx_msgs");
  summary.tx_bytes = stat_numeric_child(pipe_scope, "tx_bytes");
  summary.rx_bytes = stat_numeric_child(pipe_scope, "rx_bytes");
  return summary;
}

/// Copies one statistic node into a durable record.
inline stat_record make_stat_record(stat_view stat, std::string path,
                                    std::size_t depth) {
  stat_record record;
  record.path = std::move(path);
  record.name = stat.name();
  record.desc = stat.desc();
  record.type = stat.type();
  record.unit = stat.unit();
  record.depth = depth;
  record.timestamp_ms = stat.timestamp_ms();
  if (stat_has_numeric_value(record.type)) {
    record.numeric_value = stat.value();
  } else if (record.type == NNG_STAT_BOOLEAN) {
    record.bool_value = stat.bool_value();
  } else if (record.type == NNG_STAT_STRING) {
    const auto *value = stat.string_value();
    record.string_value = value == nullptr ? std::string{} : std::string{value};
  }
  return record;
}

/// Returns a path segment for a statistic, adding scope ids when available.
inline std::string stat_path_segment(stat_view stat) {
  std::string segment = stat.name();
  if (segment.empty()) {
    return "root";
  }
  if (stat.type() == NNG_STAT_SCOPE) {
    const auto id = stat_numeric_child(stat, "id");
    if (id.has_value()) {
      segment += "[";
      segment += std::to_string(*id);
      segment += "]";
    }
  }
  return segment;
}

/// Joins a parent statistic path and child path segment.
inline std::string join_stat_path(std::string_view parent,
                                  std::string_view segment) {
  if (parent.empty()) {
    std::string path{"/"};
    path += segment;
    return path;
  }
  std::string path{parent};
  path += "/";
  path += segment;
  return path;
}

/// Appends a flattened depth-first copy of the statistic tree.
inline void flatten_stats_into(stat_view root, std::string_view parent_path,
                               std::size_t depth,
                               std::vector<stat_record> &out) {
  if (!root.valid()) {
    return;
  }
  const auto path = join_stat_path(parent_path, stat_path_segment(root));
  out.push_back(make_stat_record(root, path, depth));
  for (auto child = root.child(); child.valid(); child = child.next()) {
    flatten_stats_into(child, path, depth + 1, out);
  }
}

/// Returns a durable depth-first copy of the statistic tree.
inline std::vector<stat_record> flatten_stats(stat_view root) {
  std::vector<stat_record> records;
  flatten_stats_into(root, "", 0, records);
  return records;
}

/// Formats one copied statistic record as diagnostic text.
inline void append_stat_record_text(const stat_record &record,
                                    std::ostringstream &out) {
  for (std::size_t i = 0; i < record.depth; ++i) {
    out << "  ";
  }
  out << record.name;
  if (record.name.empty()) {
    out << "root";
  }
  out << " type=" << stat_type_name(record.type);
  if (record.unit != NNG_UNIT_NONE) {
    out << " unit=" << stat_unit_name(record.unit);
  }
  if (record.numeric_value.has_value()) {
    out << " value=" << *record.numeric_value;
  } else if (record.bool_value.has_value()) {
    out << " value=" << (*record.bool_value ? "true" : "false");
  } else if (!record.string_value.empty()) {
    out << " value=\"" << record.string_value << "\"";
  }
  if (!record.desc.empty()) {
    out << " desc=\"" << record.desc << "\"";
  }
  out << '\n';
}

/// Formats a statistic tree for diagnostic logs.
inline std::string format_stats_tree(stat_view root) {
  std::ostringstream out;
  for (const auto &record : flatten_stats(root)) {
    append_stat_record_text(record, out);
  }
  return out.str();
}

/// Calculates counter deltas between two flattened statistic snapshots.
///
/// Counters that appear only in the current snapshot are returned with their
/// current value, which is useful when a new socket or pipe appears between
/// snapshots.
inline std::vector<stat_record>
counter_delta(const std::vector<stat_record> &previous,
              const std::vector<stat_record> &current) {
  std::unordered_map<std::string, std::uint64_t> previous_counters;
  previous_counters.reserve(previous.size());
  for (const auto &record : previous) {
    if (record.type == NNG_STAT_COUNTER && record.numeric_value.has_value()) {
      previous_counters.emplace(record.path, *record.numeric_value);
    }
  }

  std::vector<stat_record> deltas;
  for (const auto &record : current) {
    if (record.type != NNG_STAT_COUNTER || !record.numeric_value.has_value()) {
      continue;
    }
    const auto found = previous_counters.find(record.path);
    auto delta = record;
    if (found == previous_counters.end()) {
      delta.numeric_value = *record.numeric_value;
    } else {
      delta.numeric_value = *record.numeric_value >= found->second
                                ? *record.numeric_value - found->second
                                : *record.numeric_value;
    }
    deltas.push_back(std::move(delta));
  }
  return deltas;
}

NSE_NNG
