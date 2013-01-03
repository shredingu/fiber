//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_FIBERS_SOURCE

#include <boost/fiber/round_robin.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/scope_exit.hpp>

#include <boost/fiber/exceptions.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

#define RESUME_FIBER( f_) \
    BOOST_ASSERT( f_); \
    BOOST_ASSERT( ! f_->is_terminated() ); \
    f_->set_running(); \
    f_->resume();

namespace boost {
namespace fibers {

static detail::fiber_base::ptr_t extract_fiber_base(
            std::pair< detail::state_t, detail::fiber_base::ptr_t > & p)
{ return p.second; }

void
round_robin::process_fibers_()
{
    if ( fibers_.empty() ) return;

    // stable-sort has n*log(n) complexity if n*log(n) extra space is available
    std::size_t n = fibers_.size();
    std::size_t new_capacity = n * std::log10( n) + n;
    if ( fibers_.capacity() < new_capacity)
        fibers_.reserve( new_capacity);
    {
        BOOST_FOREACH( container_t::value_type const& p, fibers_)
        {
            detail::fiber_base::ptr_t f = p.second;
            int x = static_cast< int >( f->state() );
            std::cout << x << "\n";
        }
    }
    // sort fibers_ depending on state
    fibers_.sort();
    {
        BOOST_FOREACH( container_t::value_type const& p, fibers_)
        {
            detail::fiber_base::ptr_t f = p.second;
            int x = static_cast< int >( f->state() );
            std::cout << x << "\n";
        }
    }

    // copy all ready fibers to rqueue_
    std::pair< container_t::iterator, container_t::iterator > p =
            fibers_.equal_range( detail::state_ready);
    if ( p.first != p.second)
        std::transform(
            p.first, p.second,
            std::back_inserter( rqueue_),
            extract_fiber_base);

    // remove all terminated fibers from fibers_
    fibers_.erase( detail::state_terminated);
}

round_robin::round_robin() :
    active_fiber_(),
    fibers_(),
    rqueue_(),
    sleeping_()
{}

void
round_robin::spawn( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT( f);
    BOOST_ASSERT( ! f->is_terminated() );
    BOOST_ASSERT( f != active_fiber_);

    detail::fiber_base::ptr_t tmp = active_fiber_;
    BOOST_SCOPE_EXIT( & tmp, & active_fiber_) {
        active_fiber_ = tmp;
    } BOOST_SCOPE_EXIT_END
    active_fiber_ = f;
    RESUME_FIBER( active_fiber_);
    if ( ! f->is_terminated() )
        fibers_.push_back( f);
}

void
round_robin::priority( detail::fiber_base::ptr_t const& f, int prio)
{
    BOOST_ASSERT( f);

    f->priority( prio);
}

void
round_robin::join( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT( f);
    BOOST_ASSERT( ! f->is_terminated() );
    BOOST_ASSERT( f != active_fiber_);

    if ( active_fiber_)
    {
        // add active_fiber_ to joinig-list of f
        f->join( active_fiber_);
        // set active_fiber to state_waiting
        active_fiber_->set_waiting();
        // suspend active-fiber until f terminates
        active_fiber_->suspend();
        // fiber is resumed
        // f has teminated and active-fiber is resumed
    }
    else
    {
        while ( ! f->is_terminated() )
            run();
    }

    BOOST_ASSERT( f->is_terminated() );
}

void
round_robin::cancel( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT_MSG( false, "not implemented");
//  BOOST_ASSERT( f);
//  BOOST_ASSERT( f != active_fiber_);
//
//  // ignore completed fiber
//  if ( f->is_terminated() ) return;
//
//  detail::fiber_base::ptr_t tmp = active_fiber_;
//  {
//      BOOST_SCOPE_EXIT( & tmp, & active_fiber_) {
//          active_fiber_ = tmp;
//      } BOOST_SCOPE_EXIT_END
//      active_fiber_ = f;
//      // terminate fiber means unwinding its stack
//      // so it becomes complete and joining fibers
//      // will be notified
//      active_fiber_->terminate();
//  }
//  // erase completed fiber from waiting-queue
//  f_idx_.erase( f);
//
//  BOOST_ASSERT( f->is_terminated() );
}

bool
round_robin::run()
{
#if 0
    // get all fibers with reached dead-line and push them
    // at the front of runnable-queue
    sleeping_t::iterator e(
        sleeping_.upper_bound(
            schedulable( chrono::system_clock::now() ) ) );
    for (
            sleeping_t::iterator i( sleeping_.begin() );
            i != e; ++i)
    { rqueue_.push_back( i->f); } //FIXME: rqeue_.push_front() ?
    // remove all fibers with reached dead-line
    sleeping_.erase( sleeping_.begin(), e);
#endif
    if ( rqueue_.empty() )
        process_fibers_();

    // pop new fiber from runnable-queue which is not complete
    // (example: fiber in runnable-queue could be canceled by active-fiber)
    if ( rqueue_.empty() ) return false;
    detail::fiber_base::ptr_t tmp = active_fiber_;
    BOOST_SCOPE_EXIT( & tmp, & active_fiber_) {
        active_fiber_ = tmp;
    } BOOST_SCOPE_EXIT_END
    detail::fiber_base::ptr_t f( rqueue_.front() );
    rqueue_.pop_front();
    BOOST_ASSERT( f->is_ready() );
    active_fiber_ = f;
    // resume new active fiber
    RESUME_FIBER( active_fiber_);
    return true;
}

void
round_robin::wait( detail::spin_mutex::scoped_lock & lk)
{
    BOOST_ASSERT( active_fiber_);
    BOOST_ASSERT( active_fiber_->is_running() );

    // set active_fiber to state_waiting
    active_fiber_->set_waiting();
    // unlock Lock assoc. with sync. primitive
    lk.unlock();
    // suspend fiber
    active_fiber_->suspend();
    // fiber is resumed

    BOOST_ASSERT( active_fiber_->is_running() );
}

void
round_robin::yield()
{
    BOOST_ASSERT( active_fiber_);
    BOOST_ASSERT( active_fiber_->is_running() );

    // yield() suspends the fiber and adds it
    // immediately to ready-queue
    rqueue_.push_back( active_fiber_);
    // set active_fiber to state_ready
    active_fiber_->set_ready();
    // suspend fiber
    active_fiber_->yield();
    // fiber is resumed

    BOOST_ASSERT( active_fiber_->is_running() );
}

void
round_robin::sleep( chrono::system_clock::time_point const& abs_time)
{
    BOOST_ASSERT( active_fiber_);
    BOOST_ASSERT( active_fiber_->is_running() );
#if 0
    if ( abs_time > chrono::system_clock::now() )
    {
        // fiber is added with a dead-line and gets suspended
        // each call of run() will check if dead-line has reached
        sleeping_.insert( schedulable( active_fiber_, abs_time) );
        // set active_fiber to state_waiting
        active_fiber_->set_waiting();
        // suspend fiber
        active_fiber_->suspend();
        // fiber is resumed, dead-line has been reached
    }
#endif
    BOOST_ASSERT( active_fiber_->is_running() );
}

void
round_robin::migrate_to( detail::fiber_base::ptr_t const& f)
{
    BOOST_ASSERT( f);
    BOOST_ASSERT( f->is_ready() );

    rqueue_.push_back( f);
}

detail::fiber_base::ptr_t
round_robin::migrate_from()
{
    detail::fiber_base::ptr_t f;

    if ( ! rqueue_.empty() )
    {
        f.swap( rqueue_.back() );
        rqueue_.pop_back();
        BOOST_ASSERT( f->is_ready() );
    }

    return f;
}

}}

#undef RESUME_FIBER

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif