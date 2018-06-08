#pragma once

#include <deque>
#include <map>

struct Record {
    size_t _pos;
    std::string _data;

    Record() = delete;
    Record(size_t pos, const std::string& data) : _pos(pos), _data(data) {}
};

struct Queue {
    std::deque<Record> _queue;
    std::string _name;

    Queue(const std::string& name) : _name(name) {}
};

using QueuePtr = std::shared_ptr<Queue>;
using QueueMap = std::map<std::string, QueuePtr>;

void restore_queue(QueueMap& qm)
{

}