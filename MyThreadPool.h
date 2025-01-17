#pragma once
#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <optional>
#include <chrono>

struct Task
{
	std::function<void()> func;
	int priority;

	bool operator<(const Task& other) const
	{
		return  priority < other.priority;
	}
};

class MyThreadPool
{
public:
	explicit MyThreadPool(int numThreads) : stopFlag(false)
	{
		start(numThreads);
	}
	~MyThreadPool() {
		stop();
	}
	template<typename Func, typename... Args>
	auto submit(int priority, Func&& func, Args&&... args) -> std::future<typename std::result_of<Func(Args...)>::type>
	{
		using ReturnType = typename std::result_of<Func(Args...)>::type;
		auto task = std::make_shared<std::packaged_task<ReturnType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);

		// 获取 future
		std::future<ReturnType> future = task->get_future();

		// 将任务加入优先队列
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			if (stopFlag) {
				throw std::runtime_error("ThreadPool has been stopped, cannot submit tasks");
			}
			tasks.emplace(Task{ [task]() { (*task)(); }, priority });
		}

		condition.notify_one(); // 通知工作线程
		return future;
	}


private:
	std::vector<std::thread> workers;
	std::priority_queue<Task> tasks;
	std::mutex queueMutex;
	std::condition_variable condition;
	std::atomic<bool> stopFlag;

	// 启动线程池
	void start(size_t numThreads) {
		for (size_t i = 0; i < numThreads; ++i) {
			workers.emplace_back([this] {
				while (true) {
					Task task;

					// 获取任务
					{
						std::unique_lock<std::mutex> lock(queueMutex);
						condition.wait(lock, [this] { return stopFlag || !tasks.empty(); });

						if (stopFlag && tasks.empty()) {
							return; // 线程池停止，退出线程
						}

						task = tasks.top(); // 取优先队列的最高优先级任务
						tasks.pop();
					}

					// 执行任务
					task.func();
				}
				});
		}
	}

	void stop()
	{
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stopFlag = true;
		}
		condition.notify_all();
		for (std::thread& worker : workers)
		{
			if (worker.joinable()) {
				worker.join(); // 等待线程完成
			}
		}
	}
};



