#pragma once

#include <cstdio>
#include <fstream>

#include <boost/asio.hpp>

#include "metrics.h"
#include "queue.h"

bool is_num(const std::string& s)
{
    if(s.empty())
        return false;
    for(auto c : s)
        if(!std::isdigit(c))
            return false;
    return true;
}


struct CommandState {
    Metrics& _m;
    QueueMap& _qm;
    QueuePtr _q;
    size_t _p;

    boost::asio::ip::tcp::socket& _socket;
    boost::asio::io_service::strand& _strand;

    CommandState(
        Metrics& m,
        QueueMap& qm,
        boost::asio::ip::tcp::socket& socket,
        boost::asio::io_service::strand& strand
    ) : _m(m), _qm(qm), _q(nullptr), _p(0), _socket(socket), _strand(strand)
    {
    }
};

class Command
{
public:
    virtual std::string name() = 0;
    virtual std::string validate(std::vector<std::string>& tokens) = 0;
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) = 0;

    virtual ~Command() = default;
};

using Commands = std::map<std::string, std::unique_ptr<Command>>;

// class CUse : public Command
// {
// private:
//     CommandState& _s;

// public:
//     CUse(CommandState& s) : _s(s) {}

//     virtual std::string name() final { return "USE"; }
//     virtual std::string validate(std::vector<std::string>& tokens) final {
//         std::string response;
//         if(tokens.size() < 2)
//             response = "ERR not enough argument";
//         else if(tokens.size() > 2 && !is_num(tokens[2]))
//             response = "ERR queue pos must be positive integer";
//         else
//             for(auto c: tokens[1])
//                 if(!(std::isalnum(c) or c == '_'))
//                 {
//                     response = "ERR invalid queue name";
//                     break;
//                 }

//         return std::move(response);
//     }
//     virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
//         std::string response;
//         _s._m.update("session.successes." + name(), 1);

//         auto qit = _s._qm.find(tokens[1]);
//         if(qit == _s._qm.end())
//         {
//             auto p = _s._qm.emplace(tokens[1], std::make_shared<Queue>(tokens[1]));
//             qit = p.first;
//         }
//         _s._q = qit->second;

//         size_t pos = 0;
//         if(tokens.size() > 2 && !_s._q->_queue.empty())
//         {
//             pos = std::stoul(tokens[2]);
//             if(pos < _s._q->_queue.front()._pos)
//                 pos = _s._q->_queue.front()._pos;
//             if(pos > _s._q->_queue.back()._pos)
//                 pos = _s._q->_queue.back()._pos;
//         }
//         _s._p = pos;

//         return std::move(response);
//     }
// };

// class CList : public Command
// {
// private:
//     CommandState& _s;

// public:
//     CList(CommandState& s) : _s(s) {}

//     virtual std::string name() final { return "LIST"; }
//     virtual std::string validate(std::vector<std::string>& tokens) final {
//         std::string response;
//         return std::move(response);
//     }
//     virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
//         std::string response;
//         _s._m.update("session.successes." + name(), 1);

//         boost::system::error_code ec;
//         for(auto& q : _s._qm)
//         {
//             std::string qi = q.second->_name + '\t';
//             if(!q.second->_queue.empty())
//                 qi += std::to_string(q.second->_queue.front()._pos) + '\t' + std::to_string(q.second->_queue.back()._pos);
//             else
//                 qi += "\t";
//             qi += '\n';

//             boost::asio::async_write(_s._socket, boost::asio::buffer(qi.c_str(), qi.length()), yield[ec]);
//             if(ec) {
//                 response = "ERR session error";
//                 std::cerr << "session error: " << ec << std::endl;
//                 break;
//             }
//         }

//         return std::move(response);
//     }
// };

// class CQueue : public Command
// {
// private:
//     CommandState& _s;

// public:
//     CQueue(CommandState& s) : _s(s) {}

//     virtual std::string name() final { return "QUEUE"; }
//     virtual std::string validate(std::vector<std::string>& tokens) final {
//         std::string response;
//         if(_s._q == nullptr)
//             response = "ERR queue not selected";
//         return std::move(response);
//     }
//     virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
//         std::string response;
//         _s._m.update("session.successes." + name(), 1);

//         std::string qi = _s._q->_name + '\t';
//         if(!_s._q->_queue.empty())
//             qi += std::to_string(_s._q->_queue.front()._pos) + '\t' + std::to_string(_s._q->_queue.back()._pos) + '\t' + std::to_string(_s._p);
//         else
//             qi += "\t\t";
//         qi += '\n';

//         boost::system::error_code ec;
//         boost::asio::async_write(_s._socket, boost::asio::buffer(qi.c_str(), qi.length()), yield[ec]);
//         if(ec)
//         {
//             response = "ERR session error";
//             std::cerr << "session error: " << ec << std::endl;
//         }

//         return std::move(response);
//     }
// };

// class CPush : public Command
// {
// private:
//     CommandState& _s;

// public:
//     CPush(CommandState& s) : _s(s) {}

//     virtual std::string name() final { return "PUSH"; }
//     virtual std::string validate(std::vector<std::string>& tokens) final {
//         std::string response;
//         if(_s._q == nullptr)
//             response = "ERR queue not selected";
//         return std::move(response);
//     }
//     virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
//         std::string response;
//         _s._m.update("session.successes." + name(), 1);

//         boost::system::error_code ec;
//         for(size_t n = 1; n < tokens.size(); ++n)
//         {
//             size_t start_pos = 0;
//             if(!_s._q->_queue.empty())
//                 start_pos = _s._q->_queue.front()._pos;
//             Record r(start_pos + _s._q->_queue.size(), tokens[n]);

//             std::string rec_file_name = _s._q->_name + "." + std::to_string(r._pos) + "." + std::to_string(r._pos) + ".rec";
//             std::string rec_file_name_tmp = rec_file_name + ".tmp";
//             std::ofstream out(rec_file_name_tmp);
//             out << r._data << "\n";
//             out.close();

//             if(std::rename(rec_file_name_tmp.c_str(), rec_file_name.c_str()) != 0) {
//                 std::remove(rec_file_name_tmp.c_str());
//                 response = "ERR can't store data part " + std::to_string(n);
//                 break;
//             }

//             _s._q->_queue.emplace_back(std::move(r));
//             _s._strand.post(yield[ec]);
//             if(ec) {
//                 response = "ERR session error";
//                 std::cerr << "session error: " << ec << std::endl;
//                 break;
//             }
//         }

//         return std::move(response);
//     }
// };


// class CPop : public Command
// {
// private:
//     CommandState& _s;

// public:
//     CPop(CommandState& s) : _s(s) {}

//     virtual std::string name() final { return "POP"; }
//     virtual std::string validate(std::vector<std::string>& tokens) final {
//         std::string response;
//         if(_s._q == nullptr)
//             response = "ERR queue not selected";
//         return std::move(response);
//     }
//     virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
//         std::string response;
//         _s._m.update("session.successes." + name(), 1);

//         if(_s._q->_queue.empty())
//             response = "ERR queue empty";
//         else if(_s._p > _s._q->_queue.back()._pos)
//             response = "ERR no new data";
//         else if(_s._p < _s._q->_queue.front()._pos)
//             response = "ERR data lost in cursor position";
//         else {
//             const Record& r = _s._q->_queue[_s._p - _s._q->_queue.front()._pos];
//             std::string data = std::to_string(r._pos) + '\t' + r._data + '\n';
//             ++_s._p;

//             boost::system::error_code ec;
//             boost::asio::async_write(_s._socket, boost::asio::buffer(data.c_str(), data.length()), yield[ec]);
//             if(ec)
//             {
//                 response = "ERR session error";
//                 std::cerr << "session error: " << ec << std::endl;
//             }
//         }

//         return std::move(response);
//     }
// };

// class CDump : public Command
// {
// private:
//     CommandState& _s;

// public:
//     CDump(CommandState& s) : _s(s) {}

//     virtual std::string name() final { return "DUMP"; }
//     virtual std::string validate(std::vector<std::string>& tokens) final {
//         std::string response;
//         return std::move(response);
//     }
//     virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
//         std::string response;
//         _s._m.update("session.successes." + name(), 1);

//         boost::system::error_code ec;
//         bool first = true;
//         for(auto& q : _s._qm)
//         {
//             std::string qi;
//             if(!first)
//                 qi += '\n';
//             else
//                 first = false;
//             qi += q.second->_name + '\t';
//             if(!q.second->_queue.empty())
//                 qi += std::to_string(q.second->_queue.front()._pos) + '\t' + std::to_string(q.second->_queue.back()._pos);
//             else
//                 qi += "\t";
//             qi += '\n';

//             boost::asio::async_write(_s._socket, boost::asio::buffer(qi.c_str(), qi.length()), yield[ec]);
//             if(!ec) {
//                 for(auto& r: q.second->_queue) {
//                     std::string data = std::to_string(r._pos) + '\t' + r._data + '\n';
//                     boost::asio::async_write(_s._socket, boost::asio::buffer(data.c_str(), data.length()), yield[ec]);
//                     if(ec)
//                         break;
//                 }
//             }

//             if(ec)
//                 break;
//         }
//         if(!ec)
//             boost::asio::async_write(_s._socket, boost::asio::buffer("\n", 1), yield[ec]);

//         if(ec)
//         {
//             response = "ERR session error";
//             std::cerr << "session error: " << ec << std::endl;
//         }

//         return std::move(response);
//     }
// };

class CHelp : public Command
{
private:
    CommandState& _s;

public:
    CHelp(CommandState& s) : _s(s) {}

    virtual std::string name() final { return "HELP"; }
    virtual std::string validate(std::vector<std::string>& tokens) final {
        std::string response;
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        std::vector<std::string> helps;
        helps.push_back("USE queue_name [queue_pos] - switch to named queue and set specified position to continue after\n");
        helps.push_back("LIST - respond with names, sizes, 1st and last positions of queues\n");
        helps.push_back("QUEUE - respond with current queue and first, last, current positions\n");
        helps.push_back("PUSH data - add data after last record. do not move cursor\n");
        helps.push_back("POP - respond with data at cursor position. move cursor forward. error if it was last position.\n");
        helps.push_back("HELP print this text\n");

        boost::system::error_code ec;
        for(auto& h : helps)
        {
            boost::asio::async_write(_s._socket, boost::asio::buffer(h.c_str(), h.length()), yield[ec]);
            if(ec) {
                response = "ERR session error";
                std::cerr << "session error: " << ec << std::endl;
                break;
            }
        }

        return std::move(response);
    }
};
