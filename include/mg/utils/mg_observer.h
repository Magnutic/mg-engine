//**************************************************************************************************
// Mg Engine
//-------------------------------------------------------------------------------
// Copyright (c) 2018 Magnus Bergsten
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgement in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//**************************************************************************************************

/** @file mg_observer.h
 * Observer pattern implementation.
 */

#pragma once

#include <utility>

#include <mg/utils/mg_assert.h>

namespace Mg {

template<typename EventT> class Subject;

/** Interface for Observers of some subject.
 * Use `Subject<EventT>::add_observer` to make this Observer observe a given Subject.
 */
template<typename EventT> class Observer {
    friend class Subject<EventT>;

public:
    explicit Observer() {}

    virtual ~Observer() { detach(); }

    // Non-copyable
    Observer(const Observer& rhs) = delete;
    Observer& operator=(const Observer& rhs) = delete;

    // Movable
    Observer(Observer&& rhs) noexcept
    {
        m_prevs_next = rhs.m_prevs_next;
        m_next       = rhs.m_next;

        rhs.m_prevs_next = nullptr;
        rhs.m_next       = nullptr;

        if (m_prevs_next) { *m_prevs_next = this; }
        if (m_next) { m_next->m_prevs_next = &m_next; }

        sanity_check_assert();
    }

    Observer& operator=(Observer&& rhs) noexcept
    {
        Observer tmp(std::move(rhs));
        swap(*this, tmp);

        sanity_check_assert();
    }

    void swap(Observer& rhs) noexcept
    {
        using std::swap;
        swap(m_next, rhs.m_next);
        swap(m_prevs_next, rhs.m_prevs_next);

        rhs.sanity_check_assert();
        sanity_check_assert();
    }

    /** Detach observer from Subject. */
    void detach()
    {
        if (m_next) { m_next->m_prevs_next = m_prevs_next; }

        if (m_prevs_next) { *m_prevs_next = m_next; }

        m_prevs_next = nullptr;
        m_next       = nullptr;
    }

    /** Invoked by Subject::notify. */
    virtual void on_notify(const EventT& event) = 0;

    /** Invoked when subject is destroyed. Prevents Observers from being part of a dangling list. */
    void on_subject_destruction()
    {
        m_prevs_next = nullptr;
        m_next       = nullptr;
    }

private:
    void sanity_check_assert()
    {
        MG_ASSERT(!m_prevs_next || *m_prevs_next == this);
        MG_ASSERT(!m_next || m_next->m_prevs_next == &m_next);
    }

    // Observers are nodes in an intrusive linked-list.
    // `m_prevs_next` points to previous node's `m_next` pointer (or `m_head` pointer of Subject).
    Observer** m_prevs_next = nullptr;
    Observer*  m_next       = nullptr;
};

/** Subject: object which Observers observe. */
template<typename EventT> class Subject {
public:
    explicit Subject() = default;

    // Non-copyable
    Subject(const Subject&) = delete;
    Subject& operator=(const Subject&) = delete;

    // Movable
    explicit Subject(Subject&& rhs) noexcept
    {
        m_head     = rhs.m_head;
        rhs.m_head = nullptr;

        if (m_head) { m_head->m_prevs_next = &m_head; }
    }

    // Remove all Observers from list at destruction
    ~Subject()
    {
        Observer<EventT>* next = nullptr;

        for (auto cur = m_head; cur != nullptr; cur = next) {
            next = cur->m_next;
            cur->on_subject_destruction();
        }
    }

    Subject& operator=(Subject&& rhs) noexcept
    {
        Subject tmp(std::move(rhs));
        swap(*this, tmp);
        return *this;
    }

    void swap(Subject& rhs) noexcept
    {
        using std::swap;
        swap(m_head, rhs.m_head);
    }

    /** Add observer to this subject. Observer will be notified when `Subject::notify(event)` is
     * invoked.
     */
    void add_observer(Observer<EventT>& observer) noexcept
    {
        observer.detach();

        observer.m_prevs_next = &m_head;
        observer.m_next       = m_head;

        if (m_head) { m_head->m_prevs_next = &(observer.m_next); }

        m_head = &observer;
    }

    /** Invoke on_notify on all observers. */
    void notify(const EventT& e) const
    {
        Observer<EventT>* next = nullptr;

        for (auto cur = m_head; cur != nullptr; cur = next) {
            // Get next observer before invoking on_notify, in case observer detaches itself.
            next = cur->m_next;
            cur->on_notify(e);
        }
    }

private:
    Observer<EventT>* m_head = nullptr;
};

} // namespace Mg
