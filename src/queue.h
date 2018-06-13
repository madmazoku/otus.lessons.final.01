#pragma once

#include <list>
#include <deque>
#include <map>
#include <cstdio>
#include <tuple>
#include <fstream>
#include <string>

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

const size_t RECORDS_BLOCK_MAX_SIZE = 10000;
const boost::filesystem::path QUEUES_DIR = ".";
const boost::regex RB_FILE_NAME_PATTERN = boost::regex("([^\\.]+)\\.(\\d+)\\.(\\d+)\\.rec(.tmp)?");

struct Record {
    size_t _pos;
    std::string _data;

    Record() = delete;
    Record(Record&&) = default;
    Record(size_t pos, const std::string& data) : _pos(pos), _data(data) {}
};

struct RecordsBlock {
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

    void load()
    {
        std::time(&_last_access_time);

        if(!_records.empty())
            return;

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

    void unload()
    {
        _records.clear();
        _records.shrink_to_fit();
    }
};

struct Queue {
    std::deque<Record> _records;

    std::list<RecordsBlock> _blocks;
    std::string _name;

    Queue(const std::string& name) noexcept : _name(name) {}

    bool empty() const
    {
        return _records.empty() && _blocks.empty();
    }

    size_t last() const
    {
        if(!_records.empty())
            return _records.back()._pos;
        if(!_blocks.empty())
            return _blocks.back()._last;
        return 0;
    }

    size_t first() const
    {
        if(!_blocks.empty())
            return _blocks.front()._first;
        if(!_records.empty())
            return _records.front()._pos;
        return 0;
    }

    void push(const std::string& data)
    {
        size_t pos = empty() ? 0 : last() + 1;
        std::string stem = _name + "." + std::to_string(pos) + "." + std::to_string(pos);
        boost::filesystem::path rfn = QUEUES_DIR / (stem + ".rec");
        boost::filesystem::path rfnt = QUEUES_DIR / (stem + ".rec.tmp");

        std::ofstream out(rfnt.c_str());
        out << data << "\n";
        out.close();

        if(std::rename(rfnt.c_str(), rfn.c_str()) != 0)
            throw std::runtime_error("Can't rename record tmp file name");

        _records.emplace_back(pos, data);
    }

    const Record& at(size_t pos)
    {
        if(!_records.empty() && _records.front()._pos <= pos && _records.back()._pos >= pos)
            return _records[pos - _records.front()._pos];
        for(auto& rb : _blocks)
            if(rb._first <= pos && rb._last >= pos) {
                rb.load();
                return rb._records[pos - rb._first];
            }
    }
};

using QueuePtr = std::shared_ptr<Queue>;
using QueueMap = std::map<std::string, QueuePtr>;
using RecordsBlocks = std::vector<RecordsBlock>;

struct Queues {
    QueueMap _qm;

    QueuePtr queue(const std::string& name)
    {
        auto qit = _qm.find(name);
        if(qit == _qm.end()) {
            auto p = _qm.emplace(name, std::make_shared<Queue>(name));
            qit = p.first;
        }
        return qit->second;
    }

    void load()
    {
        RecordsBlocks rbs;

        for(auto itp = boost::filesystem::directory_iterator(QUEUES_DIR); itp != boost::filesystem::directory_iterator(); itp++) {
            if(!boost::filesystem::is_regular_file(itp->path()))
                continue;

            boost::cmatch groups;
            if(!boost::regex_match(itp->path().filename().c_str(), groups, RB_FILE_NAME_PATTERN))
                continue;

            std::cerr << "found: " << itp->path() << std::endl;

            rbs.emplace_back(itp->path(), groups);
        }

        std::sort(rbs.begin(), rbs.end(), [](auto& a, auto& b) {
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

            QueuePtr q = queue(rb._name);

            if(q->_blocks.empty() && rb._last == rb._first) {
                std::cerr << "New queue found: " << rb._name << std::endl;
                rb.load();
                q->_records.emplace_front(std::move(rb._records.front()));
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

            q->_blocks.emplace_front(std::move(rb));
        }

        for(auto& qp : _qm) {
            std::cerr << "queue: '" << qp.first << '"' << std::endl;
            std::cerr << "\tblocks" << std::endl;
            for(auto& rb : qp.second->_blocks) {
                std::cerr << "\t\t" << rb._path << '\t' << rb._first << '\t' << rb._last << std::endl;
                rb.load();
                for(auto& r : rb._records)
                    std::cerr << "\t\t\t" << r._pos << '\t' << r._data << std::endl;
            }
            std::cerr << "\trecords" << std::endl;
            for(auto& r : qp.second->_records)
                std::cerr << "\t\t" << r._pos << '\t' << r._data << std::endl;
            std::cerr << "\tfirst: " << qp.second->first() << "; last: " << qp.second->last() << std::endl;
            for(size_t n = qp.second->first(); n <= qp.second->last(); ++n)
                std::cerr << '\t' << "[" << n << "]: " << qp.second->at(n)._pos << " : " << qp.second->at(n)._data << std::endl;
        }
    }
};
