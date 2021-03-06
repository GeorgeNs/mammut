/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-7 by Raw Material Software ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the
   GNU General Public License, as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   JUCE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with JUCE; if not, visit www.gnu.org/licenses or write to the
   Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA

  ------------------------------------------------------------------------------

   If you'd like to release a closed-source product which uses JUCE, commercial
   licenses are also available: visit www.rawmaterialsoftware.com/juce for
   more information.

  ==============================================================================
*/

#ifndef __JUCE_MULTITIMER_JUCEHEADER__
#define __JUCE_MULTITIMER_JUCEHEADER__

#include "juce_Timer.h"
#include "../../juce_core/containers/juce_VoidArray.h"


//==============================================================================
/**
    A type of timer class that can run multiple timers with different frequencies,
    all of which share a single callback.

    This class is very similar to the Timer class, but allows you run multiple
    separate timers, where each one has a unique ID number. The methods in this
    class are exactly equivalent to those in Timer, but with the addition of
    this ID number.

    To use it, you need to create a subclass of MultiTimer, implementing the
    timerCallback() method. Then you can start timers with startTimer(), and
    each time the callback is triggered, it passes in the ID of the timer that
    caused it.

    @see Timer
*/
class JUCE_API  MultiTimer
{
protected:
    //==============================================================================
    /** Creates a MultiTimer.

        When created, no timers are running, so use startTimer() to start things off.
    */
    MultiTimer() throw();

    /** Creates a copy of another timer.

        Note that this timer will not contain any running timers, even if the one you're
        copying from was running.
    */
    MultiTimer (const MultiTimer& other) throw();

public:
    //==============================================================================
    /** Destructor. */
    virtual ~MultiTimer();

    //==============================================================================
    /** The user-defined callback routine that actually gets called by each of the
        timers that are running.

        It's perfectly ok to call startTimer() or stopTimer() from within this
        callback to change the subsequent intervals.
    */
    virtual void timerCallback (const int timerId) = 0;

    //==============================================================================
    /** Starts a timer and sets the length of interval required.

        If the timer is already started, this will reset it, so the
        time between calling this method and the next timer callback
        will not be less than the interval length passed in.

        @param timerId                  a unique Id number that identifies the timer to
                                        start. This is the id that will be passed back
                                        to the timerCallback() method when this timer is
                                        triggered
        @param  intervalInMilliseconds  the interval to use (any values less than 1 will be
                                        rounded up to 1)
    */
    void startTimer (const int timerId, const int intervalInMilliseconds) throw();

    /** Stops a timer.

        If a timer has been started with the given ID number, it will be cancelled.
        No more callbacks will be made for the specified timer after this method returns.

        If this is called from a different thread, any callbacks that may
        be currently executing may be allowed to finish before the method
        returns.
    */
    void stopTimer (const int timerId) throw();

    //==============================================================================
    /** Checks whether a timer has been started for a specified ID.

        @returns true if a timer with the given ID is running.
    */
    bool isTimerRunning (const int timerId) const throw();

    /** Returns the interval for a specified timer ID.

        @returns    the timer's interval in milliseconds if it's running, or 0 if it's no timer
                    is running for the ID number specified.
    */
    int getTimerInterval (const int timerId) const throw();


    //==============================================================================
private:
    CriticalSection timerListLock;
    VoidArray timers;

    const MultiTimer& operator= (const MultiTimer&);
};

#endif   // __JUCE_MULTITIMER_JUCEHEADER__
