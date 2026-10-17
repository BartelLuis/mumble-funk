// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license in the LICENSE file.
#ifndef MUMBLE_MURMUR_SIMPLEXLOGGER_H_
#define MUMBLE_MURMUR_SIMPLEXLOGGER_H_
#include "SimplexController.h"
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>

class SimplexLogger {
public:
	struct Config {
		QString path;
		int server            = 1;
		bool grants           = true;
		bool denials          = true;
		bool releases         = true;
		std::int64_t maxBytes = 10 * 1024 * 1024;
	};
	explicit SimplexLogger(Config config);
	~SimplexLogger();
	void submit(const SimplexController::Event &event);

private:
	Config m_config;
	std::mutex m_mutex;
	std::condition_variable m_ready;
	std::vector< SimplexController::Event > m_queue;
	bool m_stopping = false;
	std::atomic< bool > m_accepting{ true };
	std::atomic< std::uint64_t > m_dropped{ 0 };
	std::thread m_writer;
	void run();
};
#endif
