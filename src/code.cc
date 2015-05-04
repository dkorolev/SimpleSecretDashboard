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

#include <iostream>
#include "../Bricks/dflags/dflags.h"

// Stored log event structure, to parse the JSON-s.
#include "../SimpleServer/log_collector.h"

// Structured iOS events structure to follow.
#define COMPILE_MIDICHLORIANS_AS_SERVER_SIDE_CODE
#include "../MidichloriansBeta/Current/Midichlorians.h"

DEFINE_int32(port, 8687, "Port to spawn the secret server on.");
DEFINE_string(route, "/secret", "The route to serve the dashboard on.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  HTTP(FLAGS_port).Register(FLAGS_route + "/", [](Request r) { r("OK"); });

  // TOOD(dkorolev): MMQ.
  // TODO(dkorolev): Per-entries visitor.
  // TODO(dkorolev): Timer.
  // TODO(dkorolev): Uptime.
  std::string log_entry_as_string;
  LogEntry log_entry;
  std::unique_ptr<MidichloriansEvent> log_event;
  size_t i = 0, j = 0, k = 0;
  while (std::getline(std::cin, log_entry_as_string)) {
    try {
      ParseJSON(log_entry_as_string, log_entry);
      try {
        ParseJSON(log_entry.b, log_event);
        std::cerr << ++i << ' ' << j << ' ' << k << std::endl;
      } catch (const bricks::ParseJSONException &) {
        std::cerr << i << ' ' << ++j << ' ' << k << std::endl;
      }
    } catch (const bricks::ParseJSONException &) {
      std::cerr << i << ' ' << j << ' ' << ++k << std::endl;
    }
  }
}
