/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// TODO(dkorolev): Per-entries visitor.

/*
#include <iostream>
#include <algorithm>

#include <atomic>  // TODO(dkorolev): Remove this once MMQ fix is in Bricks.
#include "../Bricks/mq/inmemory/mq.h"

#include "../Bricks/rtti/dispatcher.h"

#include "../Bricks/graph/gnuplot.h"

#include "../Bricks/dflags/dflags.h"

// Stored log event structure, to parse the JSON-s.
#include "../SimpleServer/log_collector.h"

// Structured iOS events structure to follow.
#define COMPILE_MIDICHLORIANS_AS_SERVER_SIDE_CODE
#include "../MidichloriansBeta/Current/Midichlorians.h"

DEFINE_int32(port, 8687, "Port to spawn the secret server on.");
DEFINE_string(route, "/secret", "The route to serve the dashboard on.");

DEFINE_int32(initial_tick_wait_ms, 100, "");
DEFINE_int32(tick_interval_ms, 2500, "");

namespace mq {

struct State {
  const bricks::time::EPOCH_MILLISECONDS start_ms = bricks::time::Now();

  std::map<std::string, size_t> counters_total;
  std::map<std::string, size_t> counters_tick;

  uint64_t abscissa_min = static_cast<uint64_t>(-1);
  uint64_t abscissa_max = static_cast<uint64_t>(0);
  std::map<std::string, std::map<uint64_t, size_t>> events;  // Histogram [event_name][abscissa] = count.

  void IncrementCounter(const std::string& name, size_t delta = 1u) {
    counters_total[name] += delta;
    counters_tick[name] += delta;
  }

  bricks::time::MILLISECONDS_INTERVAL UptimeMs() const { return bricks::time::Now() - start_ms; }

  template <typename A>
  void save(A& ar) const {
    ar(cereal::make_nvp("uptime_ms", static_cast<uint64_t>(UptimeMs())), CEREAL_NVP(counters_total));
  }
};

struct Message {
  virtual void Process(State&) = 0;
};

struct Tick : Message {
  virtual void Process(State& state) {
    // Dump intermediate counters and reset them between ticks.
    std::cout << "uptime=" << static_cast<uint64_t>(state.UptimeMs()) / 1000 << "s";
    auto& counters = state.counters_tick;
    for (const auto cit : counters) {
      std::cout << ' ' << cit.first << '=' << cit.second;
    }
    std::cout << std::endl;
    counters.clear();
  }
  // Do something else.
};

struct Entry : Message {
  const uint64_t ms;
  const std::unique_ptr<MidichloriansEvent> entry;
  typedef std::tuple<iOSIdentifyEvent,
                     iOSDeviceInfo,
                     iOSAppLaunchEvent,
                     iOSFirstLaunchEvent,
                     iOSFocusEvent,
                     iOSGenericEvent,
                     iOSBaseEvent> T_TYPES;
  Entry() = delete;
  Entry(uint64_t ms, std::unique_ptr<MidichloriansEvent>&& entry) : ms(ms), entry(std::move(entry)) {}
  struct Processor {
    const uint64_t ms;
    State& state;
    Processor() = delete;
    Processor(uint64_t ms, State& state) : ms(ms), state(state) {}
    void operator()(const MidichloriansEvent& e) {
      state.IncrementCounter("MidichloriansEvent['" + e.device_id + "','" + e.client_id + "']");
    }
    void operator()(const iOSBaseEvent& e) { state.IncrementCounter("iosBaseEvent['" + e.description + "']"); }
    void operator()(const iOSGenericEvent& e) {
      state.IncrementCounter("iosGenericEvent['" + e.event + "','" + e.source + "']");
      const uint64_t abscissa = ms / 1000 / 60 / 60 / 24;
      ++state.events[e.event][abscissa];
      state.abscissa_min = std::min(state.abscissa_min, abscissa);
      state.abscissa_max = std::max(state.abscissa_min, abscissa);
    }
  };
  virtual void Process(State& state) {
    MidichloriansEvent& e = *entry.get();
    Processor processor(ms, state);
    bricks::rtti::RuntimeTupleDispatcher<MidichloriansEvent, T_TYPES>::DispatchCall(e, processor);
    state.IncrementCounter("entries_total");
  }
};

struct ParseErrorLogMessage : Message {
  virtual void Process(State& state) { state.IncrementCounter("entries_parse_json_error"); }
};

struct ParseErrorLogRecord : Message {
  virtual void Process(State& state) { state.IncrementCounter("entries_parse_record_error"); }
};

namespace api {

struct Status : Message {
  Request r;
  Status() = delete;
  explicit Status(Request&& r) : r(std::move(r)) {}
  virtual void Process(State& state) { r(state); }
};

struct Chart : Message {
  Request r;
  Chart() = delete;
  explicit Chart(Request&& r) : r(std::move(r)) {}
  virtual void Process(State& state) {
    using namespace bricks::gnuplot;
    if (state.abscissa_min <= state.abscissa_max) {
      const uint64_t current_abscissa = static_cast<uint64_t>(bricks::time::Now()) / 1000 / 60 / 60 / 24;
      // NOTE: `auto& plot = GNUPlot().Dot().Notation()` compiles, but fails miserably.
      GNUPlot plot;
      plot.Title("MixBoard")
          .Grid("back")
          .XLabel("Time, days ago")
          .YLabel("Number of events")
          .ImageSize(1550, 850)
          .OutputFormat("pngcairo");
      for (const auto cit : state.events) {
        // NOTE: `&cit` compiles but won't work since the method is only evaluated at render time.
        plot.Plot(WithMeta([&state, cit, current_abscissa](Plotter& p) {
                             for (uint64_t t = state.abscissa_min; t <= state.abscissa_max; ++t) {
                               const auto cit2 = cit.second.find(t);
                               p(-1.0 * (current_abscissa - t), cit2 != cit.second.end() ? cit2->second : 0);
                             }
                           })
                      .Name(cit.first)
                      .LineWidth(2.5));
      }
      r(plot);
    } else {
      r("No datapoints.");
    }
  }
};

}  // namespace mq::api

// TODO(dkorolev): Ask @mzhurovich whether this could be the default processor.
struct Consumer {
  void OnMessage(std::unique_ptr<Message>&& message) { message->Process(state); }
  State state;
};

}  // namespace mq

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  // Thread-safe sequential processing of events of multiple types, namely:
  // 1) External log entries,
  // 2) HTTP requests,
  // 3) Timer events to update console line
  mq::Consumer consumer;
  bricks::MMQ<mq::Consumer, std::unique_ptr<mq::Message>> mmq(consumer);

  HTTP(FLAGS_port).Register(FLAGS_route + "/", [](Request r) { r("OK"); });
  HTTP(FLAGS_port).Register(FLAGS_route + "/status/",
                            [&mmq](Request r) { mmq.EmplaceMessage(new mq::api::Status(std::move(r))); });
  HTTP(FLAGS_port).Register(FLAGS_route + "/chart/",
                            [&mmq](Request r) { mmq.EmplaceMessage(new mq::api::Chart(std::move(r))); });

  bool stop_timer = false;
  std::thread timer([&mmq, &stop_timer]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_initial_tick_wait_ms));
    while (!stop_timer) {
      mmq.EmplaceMessage(new mq::Tick());
      std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_tick_interval_ms));
    }
  });

  std::string log_entry_as_string;
  LogEntry log_entry;
  std::unique_ptr<MidichloriansEvent> log_event;
  while (std::getline(std::cin, log_entry_as_string)) {
    try {
      ParseJSON(log_entry_as_string, log_entry);
      try {
        ParseJSON(log_entry.b, log_event);
        mmq.EmplaceMessage(new mq::Entry(log_entry.t, std::move(log_event)));
      } catch (const bricks::ParseJSONException&) {
        mmq.EmplaceMessage(new mq::ParseErrorLogMessage());
      }
    } catch (const bricks::ParseJSONException&) {
      mmq.EmplaceMessage(new mq::ParseErrorLogRecord());
    }
  }

  stop_timer = true;
  timer.join();
}
*/

#include <vector>
#include <string>
#include <utility>

#include "html.h"

#include "../Bricks/net/api/api.h"
#include "../Bricks/dflags/dflags.h"
#include "../Bricks/util/singleton.h"
#include "../Bricks/graph/gnuplot.h"

DEFINE_int32(port, 8687, "Port to spawn the secret server on.");
DEFINE_string(route, "/secret", "The route to serve the dashboard on.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  HTTP(FLAGS_port).Register(FLAGS_route + "/", [](Request r) {
    using namespace html;
    HTML html_scope;
    {
      HEAD head;
      TITLE("Test page");
    }
    {
      BODY body;
      for (const auto i : {240, 360, 480}) {
        const auto s = bricks::strings::ToString(i);
        A a({{"href", "?dim=" + s}});
        P p("Link: " + s);
      }
      P("Hello, world!");
      {
        std::string s = "1";
        TABLE t({{"border", s}});
        {
          TR r({{"align", "right"}});
          {
            TD d;
            P("foo");
          }
          {
            TD d;
            P("bar");
          }
        }
        {
          std::vector<std::pair<std::string, std::string> > v{{"align", "center"}};
          TR r(v);
          {
            TD d;
            P("baz");
          }
          {
            TD d;
            P("meh");
          }
        }
      }
      IMG({{"src", "./image.png?dim=" + r.url.query["dim"]}});
      PRE("PRE text.");
    }
    // TODO(dkorolev): (Or John or Max) -- enable Bricks' HTTP server to send custom types via user code.
    r(html_scope.AsString(), HTTPResponseCode.OK, "text/html");
  });
  HTTP(FLAGS_port).Register(FLAGS_route + "/image.png", [](Request r) {
    const auto size = bricks::strings::FromString<size_t>(r.url.query["dim"]);
    using namespace bricks::gnuplot;
    r(GNUPlot()
          .Title("Imagine all the people ...")
          .NoKey()
          .Grid("back")
          .XLabel("... living life in peace")
          .YLabel("John Lennon, \"Imagine\"")
          .Plot(WithMeta([](Plotter p) {
                           const size_t N = 1000;
                           for (size_t i = 0; i < N; ++i) {
                             const double t = M_PI * 2 * i / (N - 1);
                             p(16 * pow(sin(t), 3),
                               -(13 * cos(t) + 5 * cos(t * 2) - 2 * cos(t * 3) - cos(t * 4)));
                           }
                         })
                    .LineWidth(5)
                    .Color("rgb '#FF0080'"))
          .ImageSize(size ? size : 500)
          .OutputFormat("pngcairo"));
  });
  HTTP(FLAGS_port).Join();
}
