#ifndef SIGSLOT_11_H_
#define SIGSLOT_11_H_
#pragma once

#include <list>
#include <set>
#include <utility>


namespace sigslot11 {

class Connectable;


class SignalInterface {
 public:
  virtual ~SignalInterface() = default;
  virtual void SlotDisconnect(Connectable *pslot) = 0;
  virtual void SlotDuplicate(const Connectable *poldslot, Connectable *pnewslot) = 0;
};

// ---

class Connectable {
 public:
  Connectable() = default;

  Connectable(Connectable const &src) {
    for (auto const &sender : src.m_senders) {
      sender->SlotDuplicate(&src, this);
      SignalConnect(sender);
    }
  }

  void SignalConnect(SignalInterface *sender) {
    m_senders.insert(sender);
  }

  void SignalDisconnect(SignalInterface *sender) {
    m_senders.erase(sender);
  }

  virtual ~Connectable() { DisconnectAll(); }

  void DisconnectAll() {
    for (auto &&sender : m_senders) sender->SlotDisconnect(this);
    m_senders.clear();
  }

 private:
  typedef typename std::set<SignalInterface *> sender_set;

 private:
  sender_set m_senders;
};

template<typename ...Args>
class ConnectionBase {
 public:
  virtual ~ConnectionBase() = default;

  virtual Connectable *GetDest() const = 0;
  virtual void Emit(Args...) = 0;
  virtual ConnectionBase *Clone() = 0;
  virtual ConnectionBase *Duplicate(Connectable *pnewdest) = 0;
};


template<typename Dest, typename ...Args>
class Connection : public ConnectionBase<Args...> {
 public:
  Connection() : m_pobject(nullptr), m_pmemfun(nullptr) {}

  Connection(Dest *pobject, void (Dest::*pmemfun)(Args...)) : m_pobject(pobject), m_pmemfun(pmemfun) {}

  ConnectionBase<Args...> *Clone() override { return new Connection(*this); }

  ConnectionBase<Args...> *Duplicate(Connectable *pnewdest) override {
    return new Connection(static_cast<Dest *>(pnewdest), m_pmemfun);
  }

  void Emit(Args... args) override { (m_pobject->*m_pmemfun)(args...); }

  Connectable *GetDest() const override { return m_pobject; }

 private:
  Dest *m_pobject;
  void (Dest::* m_pmemfun)(Args...);
};


template<typename ...Args>
class Signal : public SignalInterface {
 public:
  typedef typename std::list<ConnectionBase<Args...> *> connections_list;

  Signal() = default;

  Signal(const Signal &src) : SignalInterface(src) {
    for (auto &&slot : src.m_connected_slots) {
      slot->GetDest()->SignalConnect(this);
      m_connected_slots.push_back(slot->Clone());
    }
  }

  void SlotDuplicate(const Connectable *oldtarget, Connectable *newtarget) {
    for (auto &&slot : m_connected_slots) {
      if (slot->GetDest() != oldtarget) continue;
      m_connected_slots.push_back(slot->Duplicate(newtarget));
    }
  }

  ~Signal() override { DisconnectAll(); }

  void DisconnectAll() {
    for (auto &&slot : m_connected_slots) {
      slot->GetDest()->SignalDisconnect(this);
      delete slot;
    }
    m_connected_slots.clear();
  }

  void Disconnect(Connectable *pslot) {
    for (auto i = m_connected_slots.begin(); i != m_connected_slots.end(); ++i) {
      if ((*i)->GetDest() != pslot) continue;
      m_connected_slots.erase(i);
      pslot->SignalDisconnect(this);
      break;
    }
  }

  void SlotDisconnect(Connectable *pslot) {
    auto it = m_connected_slots.begin();
    auto itEnd = m_connected_slots.end();
    while (it != itEnd) {
      if ((*it)->GetDest() == pslot) {
        delete *it;
        it = m_connected_slots.erase(it);
      } else {
        ++it;
      }
    }
  }

  template<typename Dest>
  void Connect(Dest *pclass, void (Dest::*pmemfun)(Args...)) {
    auto *conn = new Connection<Dest, Args...>(pclass, pmemfun);
    this->m_connected_slots.push_back(conn);
    pclass->SignalConnect(this);
  }

  void Emit(Args... args) {
    for (auto &&slot : m_connected_slots) slot->Emit(args...);
  }

  void operator()(Args... args) { Emit(args...); }

 private:
  connections_list m_connected_slots;
};


}  // namespace sigslot11


#endif  // SIGSLOT_11_H_