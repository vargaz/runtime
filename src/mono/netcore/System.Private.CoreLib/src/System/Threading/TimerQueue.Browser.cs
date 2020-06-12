// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace System.Threading
{
    // Runtime interface
    internal static class WasmRuntime
    {
        private static Dictionary<int, Action> callbacks = new Dictionary<int, Action>();
        private static int next_id;

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        private static extern void SetTimeout(int timeout, int id);

        internal static void ScheduleTimeout(int timeout, Action action)
        {
            int id = ++next_id;
            callbacks[id] = action;
            SetTimeout (timeout, id);
        }

        // Called by mini-wasm.c:mono_set_timeout_exec
        private static void TimeoutCallback (int id)
        {
            Action cb = callbacks[id];
            callbacks.Remove(id);
            cb();
        }
    }

    //
    // WebAssembly-specific implementation of Timer
    // Based on TimerQueue.Portable.cs
    //
    internal partial class TimerQueue
    {
        private static List<TimerQueue>? s_scheduledTimers;
        private static List<TimerQueue>? s_scheduledTimersToFire;

        private static readonly object s_monitor = new object();

        private bool _isScheduled;
        private long _scheduledDueTimeMs;

        private TimerQueue(int id)
        {
        }

        private bool SetTimer(uint actualDuration)
        {
            Debug.Assert((int)actualDuration >= 0);
            long dueTimeMs = TickCount64 + (int)actualDuration;
            lock (s_monitor)
            {
                if (!_isScheduled)
                {
                    if (s_scheduledTimers == null)
                        s_scheduledTimers = new List<TimerQueue>(Instances.Length);
                    s_scheduledTimersToFire ??= new List<TimerQueue>(Instances.Length);
                    s_scheduledTimers.Add(this);
                    _isScheduled = true;
                }

                _scheduledDueTimeMs = dueTimeMs;
            }

            WasmRuntime.ScheduleTimeout((int)actualDuration, Run);

            return true;
        }

        private static void Run()
        {
            int shortestWaitDurationMs;
            List<TimerQueue> timersToFire = s_scheduledTimersToFire!;
            List<TimerQueue> timers;
            lock (s_monitor)
            {
                timers = s_scheduledTimers!;
                long currentTimeMs = TickCount64;
                shortestWaitDurationMs = int.MaxValue;
                for (int i = timers.Count - 1; i >= 0; --i)
                {
                    TimerQueue timer = timers[i];
                    long waitDurationMs = timer._scheduledDueTimeMs - currentTimeMs;
                    if (waitDurationMs <= 0)
                    {
                        timer._isScheduled = false;
                        timersToFire.Add(timer);

                        int lastIndex = timers.Count - 1;
                        if (i != lastIndex)
                        {
                            timers[i] = timers[lastIndex];
                        }
                        timers.RemoveAt(lastIndex);
                        continue;
                    }

                    if (waitDurationMs < shortestWaitDurationMs)
                    {
                        shortestWaitDurationMs = (int)waitDurationMs;
                    }
                }
            }

            if (timersToFire.Count > 0)
            {
                foreach (TimerQueue timerToFire in timersToFire)
                {
                    timerToFire.FireNextTimers();
                }
                timersToFire.Clear();
            }

            if (shortestWaitDurationMs != int.MaxValue)
            {
                WasmRuntime.ScheduleTimeout((int)shortestWaitDurationMs, Run);
            }
        }
    }
}
