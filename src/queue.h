#pragma once

#include <deque>
#include <map>
#include <cstdio>
#include <tuple>
#include <fstream>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

struct Record {
    size_t _pos;
    std::string _data;

    Record() = delete;
    Record(Record&&) = default;
    Record(size_t pos, const std::string& data) : _pos(pos), _data(data) {}
};

struct Queue {
    std::deque<Record> _queue;
    std::string _name;

    Queue(const std::string& name) : _name(name) {}
};

using QueuePtr = std::shared_ptr<Queue>;
using QueueMap = std::map<std::string, QueuePtr>;

const boost::regex RECORD_FILE_NAME_PATTERN = boost::regex("([^\\.]+)\\.(\\d+)\\.(\\d+)\\.rec(.tmp)?");

void restore_queue(QueueMap& qm)
{
    std::vector<std::tuple<std::string, std::string, size_t>> recs;
    boost::filesystem::path rec_dir(".");
    for(auto itp = boost::filesystem::directory_iterator(rec_dir); itp != boost::filesystem::directory_iterator(); itp++) {
        if(!boost::filesystem::is_regular_file(itp->path()))
            continue;

        boost::cmatch groups;
        if(!boost::regex_match(itp->path().filename().c_str(), groups, RECORD_FILE_NAME_PATTERN))
            continue;

        if(groups[4] == ".tmp") {
            std::remove(itp->path().c_str());
            continue;
        }

        recs.emplace_back(std::make_tuple(itp->path().string(), groups[1], std::stoul(groups[2])));
    }

    std::sort(recs.begin(), recs.end());

    for(auto& rec : recs) {
        std::string q_path;
        std::string q_name;
        size_t pos;

        std::tie(q_path, q_name, pos) = rec;

        QueuePtr q;
        auto qit = qm.find(q_name);
        if(qit == qm.end()) {
            auto p = qm.emplace(std::make_pair(q_name, std::make_shared<Queue>(q_name)));
            qit = p.first;
        }
        q = qit->second;

        std::ifstream in(q_path);
        std::string line;
        while(std::getline(in, line)) {
            if(!q->_queue.empty() && q->_queue.back()._pos >= pos)
                continue;
            if(!q->_queue.empty() && q->_queue.back()._pos + 1 != pos) {
                std::cerr << "Broken sequence in queue '" << q_name << "' at range: " << q->_queue.front()._pos << " - " << pos << std::endl;
                q->_queue.clear();
            }
            q->_queue.push_back(Record(pos, line));
            ++pos;
        }

    }
}
