#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <deque>
#include <string>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <climits>

using namespace std;

struct Process {
    string name;
    int arrival;
    int total_cpu;
    int io_every;     // do I/O after this many CPU units
    int io_len;       // how long each I/O lasts
    int remaining   = 0;
    int slice_done  = 0;  // CPU units done since last I/O
    int io_until    = 0;
    int started_at  = -1;
    int finished_at = 0;
    int wait_total  = 0;
    int ready_since = 0;

    Process(string n, int arr, int cpu, int ev, int len)
        : name(n), arrival(arr), total_cpu(cpu), io_every(ev), io_len(len),
          remaining(cpu) {}

    bool wants_io() const { return slice_done >= io_every && remaining > 0; }

    int  tat()     const { return finished_at - arrival; }
    double penalty() const { return (double)tat() / total_cpu; }
};

// min-heap comparator for SJF/SRTF - shorter job wins, name breaks ties
struct ShortestFirst {
    bool operator()(const Process* a, const Process* b) const {
        return a->remaining != b->remaining
             ? a->remaining > b->remaining
             : a->name      > b->name;
    }
};


vector<Process> load(const string& path) {
    vector<Process> v;
    ifstream f(path);
    if (!f) { cerr << "Cannot open: " << path << "\n"; exit(1); }
    string line;
    while (getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        vector<string> p;
        stringstream ss(line);
        string tok;
        while (getline(ss, tok, ';')) p.push_back(tok);
        if (p.size() != 5) continue;
        v.emplace_back(p[0], stoi(p[1]), stoi(p[2]), stoi(p[3]), stoi(p[4]));
    }
    sort(v.begin(), v.end(), [](const Process& a, const Process& b){
        return a.arrival < b.arrival || (a.arrival == b.arrival && a.name < b.name);
    });
    return v;
}


static void show_queues(int t,
                        const vector<Process*>& job,
                        const vector<string>&   ready,
                        const vector<Process*>& io,
                        Process* cpu,
                        const string& event) {

    cout << "  t=" << left << setw(4) << t << " | " << event << "\n";
    cout << "           Job   : ";
    if (job.empty()) cout << "(empty)";
    else { for (size_t i = 0; i < job.size(); i++) { if (i) cout << ", "; cout << job[i]->name; } }
    cout << "\n";

    cout << "           Ready : ";
    if (ready.empty()) cout << "(empty)";
    else { for (size_t i = 0; i < ready.size(); i++) { if (i) cout << ", "; cout << ready[i]; } }
    cout << "\n";

    cout << "           I/O   : ";
    if (io.empty()) cout << "(empty)";
    else { for (size_t i = 0; i < io.size(); i++) {
        if (i) cout << ", ";
        cout << io[i]->name << "(ends@" << io[i]->io_until << ")";
    }}
    cout << "\n";

    cout << "           CPU   : " << (cpu ? cpu->name : "IDLE") << "\n\n";
}


struct Summary { string name; double avg_tat, avg_wait, throughput; };

static Summary print_metrics(const string& algo,
                              vector<Process*>& done,
                              int total_time) {
    sort(done.begin(), done.end(), [](Process* a, Process* b){ return a->name < b->name; });

    cout << "\n  Results -- " << algo << "\n";
    cout << "  " << string(58, '-') << "\n";
    cout << "  " << left << setw(8) << "Process"
         << right << setw(8) << "Arrival" << setw(6) << "CPU"
         << setw(8) << "Finish" << setw(6) << "TAT"
         << setw(7) << "Wait" << setw(9) << "Penalty" << "\n";
    cout << "  " << string(58, '-') << "\n";

    double sum_tat = 0, sum_wait = 0;
    for (auto* p : done) {
        sum_tat  += p->tat();
        sum_wait += p->wait_total;
        cout << "  " << left << setw(8) << p->name
             << right << setw(8) << p->arrival << setw(6) << p->total_cpu
             << setw(8) << p->finished_at << setw(6) << p->tat()
             << setw(7) << p->wait_total
             << fixed << setprecision(3) << setw(9) << p->penalty() << "\n";
    }
    int n = (int)done.size();
    double avg_tat  = sum_tat  / n;
    double avg_wait = sum_wait / n;
    double thru     = (double)n / total_time;

    cout << "  " << string(58, '-') << "\n";
    cout << fixed << setprecision(2);
    cout << "  Avg Turnaround : " << avg_tat << "    Avg Wait : " << avg_wait
         << "    Throughput : " << setprecision(4) << thru << "\n\n";

    return {algo, avg_tat, avg_wait, thru};
}

static void print_comparison(const vector<Summary>& S) {
    cout << "\n  " << string(58, '=') << "\n";
    cout << "  Comparison\n";
    cout << "  " << string(58, '=') << "\n";
    cout << "  " << left << setw(16) << "Algorithm"
         << right << setw(10) << "Avg TAT" << setw(11) << "Avg Wait"
         << setw(13) << "Throughput" << "\n";
    cout << "  " << string(58, '-') << "\n";
    for (auto& s : S)
        cout << "  " << left << setw(16) << s.name
             << right << fixed << setprecision(2)
             << setw(10) << s.avg_tat << setw(11) << s.avg_wait
             << setprecision(4) << setw(13) << s.throughput << "\n";

    auto best_tat  = min_element(S.begin(), S.end(), [](auto& a, auto& b){ return a.avg_tat  < b.avg_tat;  });
    auto best_wait = min_element(S.begin(), S.end(), [](auto& a, auto& b){ return a.avg_wait < b.avg_wait; });
    cout << "\n  Best TAT  -> " << best_tat->name
         << "    Best Wait -> " << best_wait->name << "\n\n";
}


// Pull processes that have arrived into the ready structure
static void admit(vector<Process*>& job, const function<void(Process*)>& push_ready, int t) {
    vector<Process*> leftover;
    for (auto* p : job) {
        if (p->arrival <= t) { p->ready_since = t; push_ready(p); }
        else leftover.push_back(p);
    }
    job = leftover;
}

// Return processes whose I/O is done back to ready
static void collect_io(vector<Process*>& io, const function<void(Process*)>& push_ready, int t) {
    vector<Process*> leftover;
    for (auto* p : io) {
        if (p->io_until <= t) { p->ready_since = t; push_ready(p); }
        else leftover.push_back(p);
    }
    io = leftover;
}

static int earliest_event(const vector<Process*>& job, const vector<Process*>& io) {
    int t = INT_MAX;
    if (!job.empty()) t = min(t, job[0]->arrival);
    for (auto* p : io) t = min(t, p->io_until);
    return (t == INT_MAX) ? -1 : t;
}

static void dispatch_io(Process* p, int t, vector<Process*>& io) {
    p->io_until   = t + p->io_len;
    p->slice_done = 0;
    io.push_back(p);
    sort(io.begin(), io.end(), [](Process* a, Process* b){ return a->io_until < b->io_until; });
}

static vector<Process*> clone(const vector<Process>& src) {
    vector<Process*> v;
    for (auto& p : src) v.push_back(new Process(p));
    return v;
}
static void release(vector<Process*>& v) { for (auto* p : v) delete p; v.clear(); }

template<typename Container>
static vector<string> names(const Container& c) {
    vector<string> v;
    for (auto* p : c) v.push_back(p->name);
    return v;
}
static vector<string> pq_names(priority_queue<Process*, vector<Process*>, ShortestFirst> pq) {
    vector<string> v;
    while (!pq.empty()) { v.push_back(pq.top()->name); pq.pop(); }
    return v;
}


Summary run_rr(const vector<Process>& src, int quantum, bool verbose) {
    cout << "\n  == Round Robin  (quantum=" << quantum << ")  - Preemptive ==\n\n";

    auto all = clone(src);
    vector<Process*> job = all;
    deque<Process*>  ready;
    vector<Process*> io, done;
    Process* cpu   = nullptr;
    int t = 0, q = 0, gs = 0;

    auto push_ready = [&](Process* p){ ready.push_back(p); };

    while ((int)done.size() < (int)all.size()) {
        admit(job, push_ready, t);
        collect_io(io, push_ready, t);

        if (!cpu && !ready.empty()) {
            cpu = ready.front(); ready.pop_front();
            q = 0; gs = t;
            if (cpu->started_at == -1) cpu->started_at = t;
            cpu->wait_total += t - cpu->ready_since;
            if (verbose) show_queues(t, job, names(ready), io, cpu, "dispatch " + cpu->name);
        }

        if (!cpu) {
            int nxt = earliest_event(job, io);
            if (nxt == -1) break;
            t = nxt; continue;
        }

        cpu->remaining--; cpu->slice_done++; q++; t++;
        admit(job, push_ready, t);
        collect_io(io, push_ready, t);

        if (cpu->wants_io()) {
            if (verbose) show_queues(t, job, names(ready), io, cpu, cpu->name + " -> I/O");
            dispatch_io(cpu, t, io); cpu = nullptr; q = 0;

        } else if (cpu->remaining == 0) {
            cpu->finished_at = t;
            if (verbose) show_queues(t, job, names(ready), io, nullptr, cpu->name + " finished");
            done.push_back(cpu); cpu = nullptr; q = 0;

        } else if (q == quantum) {
            if (verbose) show_queues(t, job, names(ready), io, cpu, cpu->name + " preempted");
            cpu->ready_since = t;
            ready.push_back(cpu);
            cpu = nullptr; q = 0;
        }
    }

    auto s = print_metrics("Round Robin (q=" + to_string(quantum) + ")", done, t);
    release(all); return s;
}


Summary run_sjf(const vector<Process>& src, bool verbose) {
    cout << "\n  == SJF  (Shortest Job First)  - Non-Preemptive ==\n\n";

    auto all = clone(src);
    vector<Process*> job = all;
    priority_queue<Process*, vector<Process*>, ShortestFirst> ready;
    vector<Process*> io, done;
    Process* cpu = nullptr;
    int t = 0;

    auto push_ready = [&](Process* p){ ready.push(p); };

    while ((int)done.size() < (int)all.size()) {
        admit(job, push_ready, t);
        collect_io(io, push_ready, t);

        if (!cpu && !ready.empty()) {
            cpu = ready.top(); ready.pop();
            if (cpu->started_at == -1) cpu->started_at = t;
            cpu->wait_total += t - cpu->ready_since;
            if (verbose) show_queues(t, job, pq_names(ready), io, cpu,
                                     "dispatch " + cpu->name + " (rem=" + to_string(cpu->remaining) + ")");
        }

        if (!cpu) {
            int nxt = earliest_event(job, io);
            if (nxt == -1) break;
            t = nxt; continue;
        }

        cpu->remaining--; cpu->slice_done++; t++;
        admit(job, push_ready, t);
        collect_io(io, push_ready, t);

        if (cpu->wants_io()) {
            if (verbose) show_queues(t, job, pq_names(ready), io, cpu, cpu->name + " -> I/O");
            dispatch_io(cpu, t, io); cpu = nullptr;

        } else if (cpu->remaining == 0) {
            cpu->finished_at = t;
            if (verbose) show_queues(t, job, pq_names(ready), io, nullptr, cpu->name + " finished");
            done.push_back(cpu); cpu = nullptr;
        }
    }

    auto s = print_metrics("SJF", done, t);
    release(all); return s;
}


Summary run_srtf(const vector<Process>& src, bool verbose) {
    cout << "\n  == SRTF  (Shortest Remaining Time First)  - Preemptive ==\n\n";

    auto all = clone(src);
    vector<Process*> job = all;
    priority_queue<Process*, vector<Process*>, ShortestFirst> ready;
    vector<Process*> io, done;
    Process* cpu = nullptr;
    int t = 0, gs = 0;

    auto push_ready = [&](Process* p){ ready.push(p); };

    auto try_preempt = [&](int now) {
        if (cpu && !ready.empty() && ready.top()->remaining < cpu->remaining) {
            cpu->ready_since = now;
            ready.push(cpu);
            if (verbose) show_queues(now, job, pq_names(ready), io, cpu,
                                     cpu->name + " preempted");
            cpu = nullptr;
        }
    };

    while ((int)done.size() < (int)all.size()) {
        admit(job, push_ready, t);
        collect_io(io, push_ready, t);
        try_preempt(t);

        if (!cpu && !ready.empty()) {
            cpu = ready.top(); ready.pop();
            gs = t;
            if (cpu->started_at == -1) cpu->started_at = t;
            cpu->wait_total += t - cpu->ready_since;
            if (verbose) show_queues(t, job, pq_names(ready), io, cpu,
                                     "dispatch " + cpu->name + " (rem=" + to_string(cpu->remaining) + ")");
        }

        if (!cpu) {
            int nxt = earliest_event(job, io);
            if (nxt == -1) break;
            t = nxt; continue;
        }

        cpu->remaining--; cpu->slice_done++; t++;
        admit(job, push_ready, t);
        collect_io(io, push_ready, t);

        if (cpu->wants_io()) {
            if (verbose) show_queues(t, job, pq_names(ready), io, cpu, cpu->name + " -> I/O");
            dispatch_io(cpu, t, io); cpu = nullptr;

        } else if (cpu->remaining == 0) {
            cpu->finished_at = t;
            if (verbose) show_queues(t, job, pq_names(ready), io, nullptr, cpu->name + " finished");
            done.push_back(cpu); cpu = nullptr;

        } else {
            try_preempt(t);
        }
    }

    auto s = print_metrics("SRTF", done, t);
    release(all); return s;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./scheduler <input.txt> [--algo rr|sjf|srtf|all]"
                " [--quantum N] [--quiet]\n";
        return 1;
    }

    string file  = argv[1];
    string algo  = "all";
    int quantum  = 3;
    bool verbose = true;

    for (int i = 2; i < argc; i++) {
        string a = argv[i];
        if      (a == "--algo"    && i+1 < argc) algo    = argv[++i];
        else if (a == "--quantum" && i+1 < argc) quantum = stoi(argv[++i]);
        else if (a == "--quiet")                 verbose = false;
    }

    auto procs = load(file);

    cout << "\n  Processes\n  " << string(44, '-') << "\n";
    cout << "  " << left << setw(8) << "Name"
         << right << setw(8) << "Arrival" << setw(6) << "CPU"
         << setw(10) << "I/O every" << setw(8) << "I/O len" << "\n";
    cout << "  " << string(44, '-') << "\n";
    for (auto& p : procs)
        cout << "  " << left << setw(8) << p.name
             << right << setw(8) << p.arrival << setw(6) << p.total_cpu
             << setw(10) << p.io_every << setw(8) << p.io_len << "\n";
    cout << "\n";

    vector<Summary> results;
    if (algo == "rr"   || algo == "all") results.push_back(run_rr  (procs, quantum, verbose));
    if (algo == "sjf"  || algo == "all") results.push_back(run_sjf (procs, verbose));
    if (algo == "srtf" || algo == "all") results.push_back(run_srtf(procs, verbose));

    if (results.size() > 1) print_comparison(results);
    return 0;
}
