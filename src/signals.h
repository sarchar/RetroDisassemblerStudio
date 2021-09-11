#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A very basic signalling system
//
// Usage (define a signal):
//
// typedef signal<std::function<void(int, char)>> my_signal_t;
// std::shared_ptr<my_signal_t> my_signal;
// ...
// my_signal = make_shared<my_signal_t>();
//
// Usage (connect and disconnect):
//
// my_signal_t::signal_connection_t conn = object->my_signal->connect([](int a, char b){ std::cout << a << b << std::endl; });
// ...
// conn.disconnect(); // remove a connected signal
// or
// *object->my_signal += std::bind(&MyClass::Handler, this, std::placeholders::_1, std::placeholders::_2); // stays connected for the life of the object
//
// Usage (emitting):
//
// my_signal->emit(1, 'a');
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <functional>
#include <map>
#include <memory>
#include <utility>

typedef unsigned int _signal_id_t;

template <typename SignalType>
struct signal_connection {
    signal_connection(std::shared_ptr<SignalType> _signal, _signal_id_t _id)
        : signal(_signal), id(_id) { }

    ~signal_connection() {
        disconnect();
    }

    void disconnect()
    {
        if(auto s = signal.lock()) {
            s->disconnect(id);
        } 
    }

private:
    std::weak_ptr<SignalType> signal;
    _signal_id_t id;
};

// observer pattern started from
// https://stackoverflow.com/questions/13592847/c11-observer-pattern-signals-slots-events-change-broadcaster-listener-or
template <typename Func>
struct signal : public std::enable_shared_from_this<signal<Func>> {
    typedef signal_connection<signal<Func>> signal_connection_t;

    _signal_id_t next_id;

    std::map<_signal_id_t, Func> connections;

    template <typename FuncLike>
    signal_connection_t connect(FuncLike f) 
    {
        _signal_id_t id = next_id++;
        signal_connection_t conn(shared_from_this(), id);
        connections[id] = f;
        return conn;
    }

    template <typename FuncLike>
    std::shared_ptr<signal<Func>> operator+=(FuncLike f) 
    {
        _signal_id_t id = next_id++;
        connections[id] = f;
        return shared_from_this();
    }

    void disconnect(_signal_id_t id)
    {
        connections.erase(id);
    }

    template <typename ...Args>
    typename Func::result_type emit(Args... args)
    {
        for(auto& conn : connections) {
            conn.second(args...);
        }
    }
};

