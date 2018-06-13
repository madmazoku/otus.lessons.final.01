#pragma once

#include <list>
#include <deque>
#include <map>
#include <cstdio>
#include <tuple>
#include <fstream>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

const size_t RECORDS_BLOCK_MAX_SIZE = 10000;

struct Record {
    size_t _pos;
    std::string _data;

    Record() = delete;
    Record(Record&&) = default;
    Record(size_t pos, const std::string& data) : _pos(pos), _data(data) {}
};

struct RecordsBlock
{
    boost::filesystem::path _path;
    std::string _name;
    size_t _first;
    size_t _last;
    bool _tmp;
    std::vector<Record> _records;
    std::time_t _last_access_time;

    RecordsBlock() = delete;
    RecordsBlock(const boost::filesystem::path& path, const boost::cmatch& groups) noexcept : 
        _path(path), 
        _name(groups[1]), 
        _first(std::stoul(groups[2])), 
        _last(std::stoul(groups[3])),
        _tmp(groups[4] == ".tmp"),
        _last_access_time(std::time(nullptr))
    {}
    RecordsBlock(RecordsBlock&&) = default;
    RecordsBlock& operator=(RecordsBlock&&) = default;

    void load() {
        std::time(&_last_access_time);

        if(!_records.empty())
            return;
std::cerr << "Load: " << _path << std::endl;
        _records.reserve(_last - _first + 1);

        std::ifstream in(_path.string());
        std::string line;
        size_t pos = _first;
        while(std::getline(in, line) && pos <= _last)
            _records.emplace_back(pos++, line);

        _records.shrink_to_fit();

        if(_last + 1 != pos)
            throw std::runtime_error(_path.string() + " : Broken RecordsBlock, not enough data");
    }

    void unload() {
        _records.clear();
        _records.shrink_to_fit();
    }
};

struct Queue {
    std::deque<Record> _records;

    std::list<RecordsBlock> _blocks;
    std::string _name;

    Queue(const std::string& name) noexcept : _name(name) {}

    size_t last() { 
        if(!_records.empty())
            return _records.back()._pos;
        if(!_blocks.empty())
            return _blocks.back()._last;
        return 0;
    }

    size_t first() { 
        if(!_blocks.empty())
            return _blocks.front()._first;
        if(!_records.empty())
            return _records.front()._pos;
        return 0;
    }

    void push(const std::string& data) {
        _records.emplace_back(last() + 1, data);
    }

    bool empty() {
        return _records.empty() && _blocks.empty();
    }

    Record& at(size_t pos) {
        if(!_records.empty() && _records.front()._pos <= pos && _records.back()._pos >= pos)
            return _records[pos - _records.front()._pos];
        for(auto& rb : _blocks)
            if(rb._first <= pos && rb._last >= pos) {
                rb.load();
                return rb._records[pos - rb._first];
            }
        return *(Record*)nullptr;
    }
};

using QueuePtr = std::shared_ptr<Queue>;
using QueueMap = std::map<std::string, QueuePtr>;
using RecordsBlocks = std::vector<RecordsBlock>;

const boost::regex RB_FILE_NAME_PATTERN = boost::regex("([^\\.]+)\\.(\\d+)\\.(\\d+)\\.rec(.tmp)?");

RecordsBlocks read_record_blocks(const boost::filesystem::path& dir)
{
    RecordsBlocks rbs;

    boost::filesystem::path rec_dir(".");
    for(auto itp = boost::filesystem::directory_iterator(rec_dir); itp != boost::filesystem::directory_iterator(); itp++) {
        if(!boost::filesystem::is_regular_file(itp->path()))
            continue;

        boost::cmatch groups;
        if(!boost::regex_match(itp->path().filename().c_str(), groups, RB_FILE_NAME_PATTERN))
            continue;

        std::cerr << "found: " << itp->path() << std::endl;

        rbs.emplace_back(itp->path(), groups);
    }

    return rbs;
}


// void merge_queue_blocks(const std::string& queue_name, const QueueBlocks& merge)
// {
//     if(qbs.size() < 2)
//         return;

//     std::string qb_file_name = queue_name + "." + std::to_string(merge.front()._first) + "." + std::to_string(merge.back()._last) + ".rec";
//     std::string qb_file_name_tmp = rec_file_name + ".tmp";

//     std::string line;
//     std::ofstream out(qb_file_name_tmp);
//     for(auto& qb : merge) {
//         std::ifstream in(qb._path.c_str());
//         while(std::getline(in, line)) {
//             out << line << '\n';
//         }
//         in.close()
//     }
//     out.close();

// }

// void collect() {
//     QueueBlocks qbs = read_queue_blocks(".");

//     std::map<std::string, QueueBlocks> qbm;
//     for(auto& qb : qbs) {
//         auto qit = qbm.find(qb._name);
//         if(qit == qbm.end()) {
//             auto p = qbm.emplace(qb._name, QueueBlocks{});
//             qit = p.first;
//         }
//         qit->second.push_back(qb);
//     }

//     for(auto& qbmp : qbm) {
//         QueueBlocks merge;
//         for(auto& qb : qbmp.second) {
//             if(qb._tmp)
//                 continue;
//             if(!merge.empty() && )
//             if(!merge.empty() && merge.back()._last + 1 != qb._first) {
//                 std::cerr << "Broken sequence in queue '" << qb._name << "' at range: " << q->_queue.back()._pos << " - " << pos << std::endl;
//                 merge_queue_blocks(qbmp.first, merge);
//                 merge.clear();
//             }
//             size_t qb_size = qb._last - qb._first + 1;
//             size_t merge_size = merge.empty() ? 0 : merge.back()._last - merge.front()._first + 1;
//             if(qb_size + merge_size >= QUEUE_BLOCK_MAX_SIZE) {
//                 merge_queue_blocks(qbmp.first, merge);
//                 merge.clear();
//                 continue;
//             }
//             if(qb_size < QUEUE_BLOCK_MAX_SIZE)
//                 merge.push_back(qb);
//         }
//         merge_queue_blocks(qbmp.first, merge);
//     }

// }

void restore_queue(QueueMap& qm)
{
    RecordsBlocks rbs = read_record_blocks(".");

    std::sort(rbs.begin(), rbs.end(), [](auto& a, auto& b){
        return 
            a._name == b._name ? 
                a._last == b._last ?
                    a._first == b._first ? 
                        a._tmp & !b._tmp
                        : a._first < b._first
                    : a._last > b._last
                : a._name < b._name;
    });

    std::map<std::string, RecordsBlocks> queues;
    for(auto& rb : rbs) {
        std::cerr << "block: " << rb._path << std::endl;
        if(rb._tmp) {
            std::remove(rb._path.c_str());
            continue;
        }

        QueuePtr q;
        auto qit = qm.find(rb._name);
        if(qit == qm.end()) {
            auto p = qm.emplace(rb._name, std::make_shared<Queue>(rb._name));
            qit = p.first;
        }
        q = qit->second;

        if(q->_blocks.empty() && rb._last == rb._first) {
            std::cerr << "New queue found: " << rb._name << std::endl;
            rb.load();
            q->_records.emplace_back(std::move(rb._records.front()));
            continue;
        }

        if(rb._first >= q->_blocks.front()._first && rb._last <= q->_blocks.front()._last) {
            std::cerr << "Internal block found: " << rb._path << std::endl;
            // std::remove(rb._path.c_str());
            continue;
        }

        if(!q->empty() && rb._last + 1 != q->first()) {
            std::cerr << "Broken sequence in queue '" << rb._name << std::endl;
            break;
        }

        q->_blocks.emplace_back(std::move(rb));
    }

    for(auto& qp : qm) {
        std::cerr << "queue: '" << qp.first << '"' << std::endl;
        for(size_t n = qp.second->first(); n != qp.second->last(); ++n)
            std::cerr << '\t' << "[" << n << "]: " << qp.second->at(n)._pos << " : " << qp.second->at(n)._data << std::endl;
    }

}
