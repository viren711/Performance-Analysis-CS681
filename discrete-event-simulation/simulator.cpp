#include <iostream>
#include <queue>
#include <vector>
#include <cmath>
#include <random>
#include <iomanip>

using namespace std;

/* ================= CONFIG ================= */

class Config
{
public:

    int users = 50;
    int cores = 4;
    int threads = 100;
    int buffer = 500;

    double service_mean = 0.8;
    double think_mean = 3;

    double timeout = 5;

    double quantum = 0.2;
    double context_switch = 0.01;

    double sim_time = 10000;
    double warmup = 2000;
};

/* ================= EVENT ================= */

enum EventType
{
    ARRIVAL,
    DISPATCH,
    TIMEOUT
};

class Event
{
public:

    double time;
    EventType type;
    int request;

    Event(double t, EventType ty, int r)
    {
        time = t;
        type = ty;
        request = r;
    }
};

class EventCompare
{
public:

    bool operator()(Event a, Event b)
    {
        return a.time > b.time;
    }
};

/* ================= REQUEST ================= */

class Request
{
public:

    int id;
    double arrival;
    double remaining_service;

    bool finished = false;
    bool timed_out = false;

    Request(int i,double t,double s)
    {
        id=i;
        arrival=t;
        remaining_service=s;
    }
};

/* ================= METRICS ================= */

class Metrics
{
public:

    int goodput=0;
    int badput=0;
    int dropped=0;

    double response_sum=0;

    double busy_time=0;
};

/* ================= RANDOM ================= */

class RNG
{
public:

    default_random_engine gen;

    RNG(int seed)
    {
        gen.seed(seed);
    }

    double exponential(double mean)
    {
        exponential_distribution<double> dist(1.0/mean);
        return dist(gen);
    }

    double uniform(double a,double b)
    {
        uniform_real_distribution<double> dist(a,b);
        return dist(gen);
    }
};

/* ================= CORE ================= */

class Core
{
public:

    bool busy=false;
    int current_request=-1;
};

/* ================= SIMULATOR ================= */

class Simulator
{

public:

    Config cfg;

    RNG rng;

    double sim_time=0;

    priority_queue<Event,vector<Event>,EventCompare> eventList;

    vector<Request> requests;

    vector<Core> cores;

    queue<int> threadQueue;

    Metrics metrics;

    Simulator(Config c,int seed):cfg(c),rng(seed)
    {
        cores.resize(cfg.cores);
    }

    /* ================= INITIALIZATION ================= */

    void initialize()
    {

        for(int i=0;i<cfg.users;i++)
        {
            double t=rng.exponential(cfg.think_mean);

            double service=rng.exponential(cfg.service_mean);

            requests.push_back(Request(i,t,service));

            eventList.push(Event(t,ARRIVAL,i));
        }

    }

    /* ================= FIND FREE CORE ================= */

    int freeCore()
    {
        for(int i=0;i<cores.size();i++)
            if(!cores[i].busy)
                return i;

        return -1;
    }

    /* ================= ARRIVAL ================= */

    void handleArrival(Event e)
    {

        int id=e.request;

        requests[id].remaining_service=rng.exponential(cfg.service_mean);

        requests[id].arrival=sim_time;

        if(threadQueue.size()<cfg.threads)
        {
            threadQueue.push(id);
            scheduleDispatch();
        }
        else
        {
            metrics.dropped++;
        }

    }

    /* ================= DISPATCH ================= */

    void scheduleDispatch()
    {

        int core=freeCore();

        if(core==-1) return;

        if(threadQueue.empty()) return;

        int req=threadQueue.front();
        threadQueue.pop();

        cores[core].busy=true;
        cores[core].current_request=req;

        double slice=min(cfg.quantum,requests[req].remaining_service);

        requests[req].remaining_service-=slice;

        metrics.busy_time+=slice;

        eventList.push(Event(sim_time+slice+cfg.context_switch,DISPATCH,core));
    }

    /* ================= DISPATCH EVENT ================= */

    void handleDispatch(Event e)
    {

        int core=e.request;

        int req=cores[core].current_request;

        if(requests[req].remaining_service<=0)
        {

            if(!requests[req].timed_out)
            {
                metrics.goodput++;

                if(sim_time>cfg.warmup)
                    metrics.response_sum+=sim_time-requests[req].arrival;
            }
            else
            {
                metrics.badput++;
            }

            double think=rng.exponential(cfg.think_mean);

            eventList.push(Event(sim_time+think,ARRIVAL,req));

            cores[core].busy=false;
            cores[core].current_request=-1;

        }
        else
        {
            threadQueue.push(req);

            cores[core].busy=false;
            cores[core].current_request=-1;
        }

        scheduleDispatch();

    }

    /* ================= TIMEOUT ================= */

    void handleTimeout(Event e)
    {
        int id=e.request;
        requests[id].timed_out=true;
    }

    /* ================= RUN ================= */

    void run()
    {

        while(!eventList.empty())
        {

            Event e=eventList.top();
            eventList.pop();

            if(e.time>cfg.sim_time)
                break;

            sim_time=e.time;

            if(e.type==ARRIVAL)
                handleArrival(e);

            else if(e.type==DISPATCH)
                handleDispatch(e);

            else if(e.type==TIMEOUT)
                handleTimeout(e);

        }

    }

};

/* ================= MULTIPLE RUNS ================= */

Metrics runMultiple(Config cfg,int runs)
{

    Metrics avg;

    for(int i=0;i<runs;i++)
    {

        Simulator sim(cfg,i+1);

        sim.initialize();
        sim.run();

        avg.goodput+=sim.metrics.goodput;
        avg.badput+=sim.metrics.badput;
        avg.dropped+=sim.metrics.dropped;

        avg.response_sum+=sim.metrics.response_sum;

        avg.busy_time+=sim.metrics.busy_time;
    }

    avg.goodput/=runs;
    avg.badput/=runs;
    avg.dropped/=runs;

    avg.response_sum/=runs;
    avg.busy_time/=runs;

    return avg;

}

/* ================= EXPERIMENT ================= */

void usersExperiment()
{

    Config cfg;

    cout<<"users,response,throughput,goodput,badput,drop,utilization\n";

    for(int u=5;u<=100;u+=5)
    {

        cfg.users=u;

        Metrics m=runMultiple(cfg,20);

        double response=m.response_sum/max(1,m.goodput);

        double goodput=m.goodput/cfg.sim_time;
        double badput=m.badput/cfg.sim_time;

        double throughput=goodput+badput;

        double util=m.busy_time/(cfg.sim_time*cfg.cores);

        cout<<u<<","
            <<response<<","
            <<throughput<<","
            <<goodput<<","
            <<badput<<","
            <<m.dropped<<","
            <<util<<"\n";
    }

}

void contextSwitchExperiment()
{
    Config cfg;

    cfg.users = 40;
    cfg.threads = 100;

    cout << "ctxswitch,response,throughput\n";

    for(double cs=0.001; cs<=0.1; cs+=0.01)
    {
        cfg.context_switch = cs;

        Metrics m = runMultiple(cfg,20);

        double response = m.response_sum / max(1,m.goodput);

        double throughput = (m.goodput + m.badput) / cfg.sim_time;

        cout << cs << ","
             << response << ","
             << throughput << "\n";
    }
}

/* ================= MAIN ================= */

int main()
{

    //usersExperiment();
    contextSwitchExperiment();
}