11:40 PM 2/7/2019

pent0:

    This references the Symbian SMP scheduling, allowing multi-core model for EKA2L1 without any changes to 
    kernel interface.

    Notes:
    - It's from what I understand by reading the kernel code. Should clarify that this is a reimplementation,
    and I didn't reverse anything (I rarely do though, since version that I'm currently working on s60v5, isn't
    much different in term of services from s^3)

    Please point out any accuracies. The most dangerous place I think we need to keep an eye to is:
    - The balancer.

    How it works internally, from what i observed:

    1. Concept
        - Subscheduler: A scheduler for a single core. Each subscheduler has its own run queue, and timer.
        It also has its own rebalance queue.
        - Concept of scheduable: a single or group of thread that can be queued for schedule and running by
        the subscheduler.
        - Thread group: inhertated from scheduable, but this time it's a group.
        - Load: The amount of traffic on a core.
        - Load balancer: A act that balances the load on all cores for fairness. For example, 5 scheduable on core 0,
        vs 1 scheduable on core 1, we already see heavy load difference. The balancer will make the load more fair.

    2. Sub scheduler. (4:20 PM 3/7/2019)
        - The sub scheduler works pretty much the same as the scheduler on single core.
        - Each sub-scheduler contains 64 linked list queue (each queue is for different thread priority). Example,
        scheduable with priority 5 will be added to the back 5th queue. This is also a prepare step for round-robin,
        a scheduling algorithm with the roundabout model (FIFO with the array being a ring).
        - The list also comes with two 32-bit mask. It marks which queue is not empty with bit 1. This will comes handy
        in the future
        - We don't talk about details about how it does its hardware and also nanokernel jobs, I will just describe
        the main process.
        - First, the sub scheduler will look for the highest-priority queue that is not empty. This can be easily done
        by finding the most significant bit in those masks.
        - We got the scheduable. If the scheduable is a group, then first thread of the group will be taken. I think you got it.
        - The sub scheduler maybe will take this as the current thread for the core, but first it has to check for the timeslice.
        If the timeslice (the amount ticks that the thread supposed to run), is zero, then the next thread that is ready will be the
        one in charge, and the current thread will be added to the end of the queue it resigned in.
        - Thread that is being waited for suspened has already been removed from those queue, so don't worry.
        
    3. CPU load. (4:53 PM 3/7/2019)
        - A load represents the amount of traffic on a CPU.
        - Each CPU core has maximum 4095 units of load. Each scheduable of the core has the load units calculated by
        the time it has run between 2 rebalance. How it's calculated, we will see later.
        - Based on the loads, the balancer can pick which CPU new scheduable should be resigned on, by choosing the
        core that is available and has the smallest load of all.
        - On SMP kernel, the struct is SCpuAvailability. There is an array named iRemains with number of components
        equal to maximum core. The remain means remaining unit for each core. iRemains are initially constructed with
        4095 (max). If the remain equals to 4095, it means that no load is on the core, or we considered it EIdle

    4. Load balancer (6:01 PM 3/7/2019)
       Code (2009): https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv/blob/master/kernel/eka/nkernsmp/nk_bal.cpp
        
        a) Basic

        - You should use some metrics program (like Core Temp on Windows) before, so you can see that the load
        is gravel equally across all cores. Core 0 maybe a bit of greedy (since it's main), but usually that's the ideal.
        - When a new scheduable is queued to the sub-scheduler for the first time, it's also taken into custody by
        the sub-scheduler's load balancing queue. Remember that.
        - The kernel has a timer that calls periodic rebalance every 107 nanokernel ticks (they should calculated it
        percisely from user usage). Where the timer runs is currently not known to me, but it doesn't matter in our context.
        - The rebalance first queries all scheduable group from all load rebalance queue of both parent scheduler (TScheduler),
        and sub-scheduler, to a single queue. When queueing is done, the real balancing begins.
        - Iterates through each scheduable, and calculate it stats. The stats include:
            * Recent run time, active time.
            * Running ticks, active ticks between 2 periodic balance, in load unit.
            * Hot and warm status (run and active status, think of it like a human body).  Important component.
            * The weight. This is a important component, decided by the priority of the scheduable. We will talk
              about this later.
        - Next, is where it decided what to balance.
            * If the warm is under a defined number of ticks, it means that it hasn't been actived for a while.
              The load balance status for the scheduable is reset to inactive state, and it will stay on the core, since
              the load is just incrediable small. However, if the core is shutting down, it will be moved to another core,
              since it's not dead yet.
            * If only one core is active, no rebalance is needed. It's still being kept on the rebalance queue though,
              in case another core turn on.
            * If the scheduable mark as cycles between core (afaik only see it in driver), don't rebalance, but still kept it
              on the queue.
            * Other than that, the scheduable is added to a sorted queue for futher look. How to queue is sorted will be describe,
              here!
                 + The queue is divied into two parts: at the beginning, is non-heavy scheduable, and at the end, is heavy
                 scheduable.
                 + Non-heavy are sorted from largest to smallest by its average run time between 2 perodic balance, whether
                 heavy are reversed, by its average run and active time.
            * Iterates through the sorted queue. Now this's where to CPU load struct comes in. If the comp is non-heavy,
            the load struct is used to pick the smallest load core, and throw the scheduable in there. If the comp is heavy,
            it must do some extra work. Check the total heavy scheduable left. If the number of them smaller then total core
            active, we can throw each of them in a core, else we have to gravel them across all cores. Core that are picked by
            heavy one will have load marked as maxed out.
            * In case all core loads are maxed out, it will just pick the highest core available.
            * When a scheduable changes core, reschedule will be marked as needed.

        b)  Calculation (8:39 PM 3/7/2019)
            - Calculate runtime and active load unit:
                * Needed input includes: delta time, delta run, and delta active (between 2 periodic rebalance),
                They are all clamped down to 2^20, by shifting.
                * Unit = ((target_delta * 4095) + (delta_time / 2)) / delta_time.
                * Since delta run and active is always smaller than delta time, target_delta / delta_time is something
                like a ratio. And 4095 is the maximum load unit. But since unit is an integer, doing this has a risk
                of unit being 0 while it's not suppose to. So delta_time / 2 is an insurance.
            
            - Calculate hot and warm through given time argument
                * 1 Hot warm unit = the CPU cycles per ticks. (afaik it's FastCounterFrequency / 1.000.000)
                * First, the time given is divided with the 1 hot warm unit.
                * In case the unit count is too large (the high part is not empty), the return number is 255.
                * Else, the unit are square up (unit * unit), and the return number is the position of most significant
                  bit 1

            - Calculate the heaviness:
                * Symbian decide that a scheduable is heavy if it's priority is up from 25 (not equal to 25).
                * iLbHeavy is set to 0xff in case the scheduable is not heavy, else it's set to 0
                * The time the thread actives also affect its heaviness, if it's active up to 90% maximum load
                unit, then iLbHeavy is or with 0x80 (random number?)
                * Symbian decides if a scheduable is heavy or not, by:
                    + XORing 0xff with iLbHeavy. (result is x)
                    + Clear the least significant bit (x & (x-1)), and check if it's more than 0, then it's heavy