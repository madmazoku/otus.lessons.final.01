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
    Queues& _qs;
    QueuePtr _q;
    size_t _p;

    boost::asio::ip::tcp::socket& _socket;
    boost::asio::io_service::strand& _strand;

    CommandState(
        Metrics& m,
        Queues& qs,
        boost::asio::ip::tcp::socket& socket,
        boost::asio::io_service::strand& strand
    ) : _m(m), _qs(qs), _q(nullptr), _p(0), _socket(socket), _strand(strand)
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

class CUse : public Command
{
private:
    CommandState& _s;

public:
    CUse(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "USE";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        if(tokens.size() < 2)
            response = "ERR not enough argument";
        else if(tokens.size() > 2 && !is_num(tokens[2])) {
            boost::to_upper(tokens[2]);
            if(tokens[2] != "NEW" && tokens[2] != "LAST" && tokens[2] != "FIRST")
                response = "ERR queue pos must have positive integer, 'NEW', LAST' or 'FIRST' value";
        }

        if(response.empty())
            for(auto c: tokens[1])
                if(!(std::isalnum(c) or c == '_')) {
                    response = "ERR invalid queue name";
                    break;
                }

        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        _s._q = _s._qs.queue(tokens[1]);

        if(_s._q->empty())
            _s._p = 0;
        else if(tokens.size() <= 2 || tokens[2] == "FIRST")
            _s._p = _s._q->first();
        else if(tokens[2] == "LAST")
            _s._p = _s._q->last();
        else if(tokens[2] == "NEW")
            _s._p = _s._q->last() + 1;
        else
            _s._p = std::stoul(tokens[2]);

        return std::move(response);
    }
};

class CList : public Command
{
private:
    CommandState& _s;

public:
    CList(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "LIST";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        boost::system::error_code ec;
        for(auto& q : _s._qs._qm) {
            std::string qi = q.second->_name + '\t';
            if(!q.second->empty())
                qi += std::to_string(q.second->first()) + '\t' + std::to_string(q.second->last());
            else
                qi += "\t";
            qi += '\n';

            boost::asio::async_write(_s._socket, boost::asio::buffer(qi.c_str(), qi.length()), yield[ec]);
            if(ec) {
                response = "ERR session error";
                std::cerr << "session error: " << ec << std::endl;
                break;
            }
        }

        return std::move(response);
    }
};

class CQueue : public Command
{
private:
    CommandState& _s;

public:
    CQueue(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "QUEUE";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        if(_s._q == nullptr)
            response = "ERR queue not selected";
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        std::string qi = _s._q->_name + '\t';
        if(!_s._q->empty())
            qi += std::to_string(_s._q->first()) + '\t' + std::to_string(_s._q->last()) + '\t' + std::to_string(_s._p);
        else
            qi += "\t\t";
        qi += '\n';

        boost::system::error_code ec;
        boost::asio::async_write(_s._socket, boost::asio::buffer(qi.c_str(), qi.length()), yield[ec]);
        if(ec) {
            response = "ERR session error";
            std::cerr << "session error: " << ec << std::endl;
        }

        return std::move(response);
    }
};

class CPush : public Command
{
private:
    CommandState& _s;

public:
    CPush(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "PUSH";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        if(_s._q == nullptr)
            response = "ERR queue not selected";
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        boost::system::error_code ec;
        for(size_t n = 1; n < tokens.size(); ++n) {
            _s._q->push(tokens[n]);

            _s._strand.post(yield[ec]);
            if(ec) {
                response = "ERR session error";
                std::cerr << "session error: " << ec << std::endl;
                break;
            }
        }

        return std::move(response);
    }
};

class CPop : public Command
{
private:
    CommandState& _s;

public:
    CPop(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "POP";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        if(_s._q == nullptr)
            response = "ERR queue not selected";
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        if(_s._q->empty())
            response = "ERR queue empty";
        else if(_s._p > _s._q->last())
            response = "ERR no new data";
        else if(_s._p < _s._q->first())
            response = "ERR data lost in cursor position";
        else {
            const Record& r = _s._q->at(_s._p);
            std::string data = std::to_string(r._pos) + '\t' + r._data + '\n';
            ++_s._p;

            boost::system::error_code ec;
            boost::asio::async_write(_s._socket, boost::asio::buffer(data.c_str(), data.length()), yield[ec]);
            if(ec) {
                response = "ERR session error";
                std::cerr << "session error: " << ec << std::endl;
            }
        }

        return std::move(response);
    }
};

class CDump : public Command
{
private:
    CommandState& _s;

public:
    CDump(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "DUMP";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        boost::system::error_code ec;
        bool first = true;
        for(auto& q : _s._qs._qm) {
            std::string qi;
            if(!first)
                qi += '\n';
            else
                first = false;
            qi += q.second->_name + '\t';
            if(!q.second->empty())
                qi += std::to_string(q.second->first()) + '\t' + std::to_string(q.second->last());
            else
                qi += "\t";
            qi += '\n';

            boost::asio::async_write(_s._socket, boost::asio::buffer(qi.c_str(), qi.length()), yield[ec]);
            if(ec)
                break;

            if(!q.second->empty())
                for(size_t n = q.second->first(); n <= q.second->last(); ++n) {
                    const Record& r = q.second->at(n);
                    std::string data = std::to_string(r._pos) + '\t' + r._data + '\n';
                    std::cerr << '\t' << data;
                    boost::asio::async_write(_s._socket, boost::asio::buffer(data.c_str(), data.length()), yield[ec]);
                    if(ec)
                        break;
                }

            if(ec)
                break;
        }
        if(!ec)
            boost::asio::async_write(_s._socket, boost::asio::buffer("\n", 1), yield[ec]);

        if(ec) {
            response = "ERR session error";
            std::cerr << "session error: " << ec << std::endl;
        }

        return std::move(response);
    }
};

class CHelp : public Command
{
private:
    CommandState& _s;

public:
    CHelp(CommandState& s) : _s(s) {}

    virtual std::string name() final
    {
        return "HELP";
    }
    virtual std::string validate(std::vector<std::string>& tokens) final
    {
        std::string response;
        return std::move(response);
    }
    virtual std::string execute(std::vector<std::string>& tokens, boost::asio::yield_context& yield) final
    {
        std::string response;
        _s._m.update("session.successes." + name(), 1);

        std::vector<std::string> helps;
        helps.push_back("USE queue_name [pos] - switch to named queue and set specified position to continue after, pos may be number, 'FIRST', 'LAST' or 'NEW'\n");
        helps.push_back("LIST - respond with names, sizes, 1st and last positions of queues\n");
        helps.push_back("QUEUE - respond with current queue and first, last, current positions\n");
        helps.push_back("PUSH data - add data after last record. do not move cursor\n");
        helps.push_back("POP - respond with data at cursor position. move cursor forward. error if it was last position.\n");
        helps.push_back("HELP print this text\n");

        boost::system::error_code ec;
        for(auto& h : helps) {
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
