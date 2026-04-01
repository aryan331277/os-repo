#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <string>
#include <algorithm>
#include <iomanip>
#include <climits>
using namespace std;
struct Process {
    string name;
    int arrival;       
    int total_cpu;     
    int io_every;      
    int io_len;        
    int remaining;     
    int slice_done;    
    int io_until;      
    int started_at;    
    int finished_at;   
    int wait_total;    
    int ready_since;   
    Process(string n, int arr, int cpu, int ev, int len) {
        name        = n;
        arrival     = arr;
        total_cpu   = cpu;
        io_every    = ev;
        io_len      = len;
        remaining   = cpu;
        slice_done  = 0;
        io_until    = 0;
        started_at  = -1;
        finished_at = 0;
        wait_total  = 0;
        ready_since = 0;
    }
    bool wants_io() {
        return slice_done >= io_every && remaining > 0;
    }
    int tat() {
        return finished_at - arrival;
    }
    double penalty() {
        return (double)tat() / total_cpu;
    }
};
struct ShortestFirst {
    bool operator()(Process* a, Process* b) {
        if (a->remaining != b->remaining) {
            return a->remaining > b->remaining; 
        }
        return a->name > b->name; 
    }
};
struct Summary {
    string name;
    double avg_tat;
    double avg_wait;
    double throughput;
};
vector<Process> load(string path) {
    vector<Process> list;
    ifstream file(path);
    if (!file) {
        cout << "Cannot open file: " << path << "\n";
        exit(1);
    }
    string line;
    while (getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') continue;
        vector<string> parts;
        stringstream ss(line);
        string token;
        while (getline(ss, token, ';')) {
            parts.push_back(token);
        }
        if (parts.size() != 5) continue;
        Process p(parts[0], stoi(parts[1]), stoi(parts[2]), stoi(parts[3]), stoi(parts[4]));
        list.push_back(p);
    }
    for (int i = 0; i < (int)list.size(); i++) {
        for (int j = i + 1; j < (int)list.size(); j++) {
            if (list[j].arrival < list[i].arrival ||
               (list[j].arrival == list[i].arrival && list[j].name < list[i].name)) {
                swap(list[i], list[j]);
            }
        }
    }
    return list;
}
void show_queues(int t, vector<Process*> job, vector<Process*> ready,
                 vector<Process*> io, Process* cpu, string event) {
    cout << "  t=" << t << " | " << event << "\n";
    cout << "    Job   : ";
    if (job.empty()) cout << "(empty)";
    for (int i = 0; i < (int)job.size(); i++) {
        if (i) cout << ", ";
        cout << job[i]->name;
    }
    cout << "\n";
    cout << "    Ready : ";
    if (ready.empty()) cout << "(empty)";
    for (int i = 0; i < (int)ready.size(); i++) {
        if (i) cout << ", ";
        cout << ready[i]->name;
    }
    cout << "\n";
    cout << "    I/O   : ";
    if (io.empty()) cout << "(empty)";
    for (int i = 0; i < (int)io.size(); i++) {
        if (i) cout << ", ";
        cout << io[i]->name << "(ends@" << io[i]->io_until << ")";
    }
    cout << "\n";
    cout << "    CPU   : ";
    if (cpu) cout << cpu->name;
    else cout << "IDLE";
    cout << "\n\n";
}
Summary print_metrics(string algo, vector<Process*> done, int total_time) {
    for (int i = 0; i < (int)done.size(); i++) {
        for (int j = i + 1; j < (int)done.size(); j++) {
            if (done[j]->name < done[i]->name) swap(done[i], done[j]);
        }
    }
    cout << "\n  Results -- " << algo << "\n";
    cout << "  " << string(58, '-') << "\n";
    cout << "  " << left << setw(8) << "Process"
         << right << setw(8) << "Arrival" << setw(6) << "CPU"
         << setw(8) << "Finish" << setw(6) << "TAT"
         << setw(7) << "Wait" << setw(9) << "Penalty" << "\n";
    cout << "  " << string(58, '-') << "\n";
    double sum_tat  = 0;
    double sum_wait = 0;
    for (int i = 0; i < (int)done.size(); i++) {
        Process* p = done[i];
        sum_tat  += p->tat();
        sum_wait += p->wait_total;
        cout << "  " << left << setw(8) << p->name
             << right << setw(8) << p->arrival
             << setw(6) << p->total_cpu
             << setw(8) << p->finished_at
             << setw(6) << p->tat()
             << setw(7) << p->wait_total
             << fixed << setprecision(3) << setw(9) << p->penalty() << "\n";
    }
    int    n        = done.size();
    double avg_tat  = sum_tat  / n;
    double avg_wait = sum_wait / n;
    double thru     = (double)n / total_time;
    cout << "  " << string(58, '-') << "\n";
    cout << fixed << setprecision(2);
    cout << "  Avg Turnaround : " << avg_tat
         << "    Avg Wait : " << avg_wait
         << "    Throughput : " << setprecision(4) << thru << "\n\n";
    Summary s;
    s.name       = algo;
    s.avg_tat    = avg_tat;
    s.avg_wait   = avg_wait;
    s.throughput = thru;
    return s;
}
void print_comparison(vector<Summary> results) {
    cout << "\n  " << string(58, '=') << "\n";
    cout << "  Comparison\n";
    cout << "  " << string(58, '=') << "\n";
    cout << "  " << left << setw(16) << "Algorithm"
         << right << setw(10) << "Avg TAT"
         << setw(11) << "Avg Wait"
         << setw(13) << "Throughput" << "\n";
    cout << "  " << string(58, '-') << "\n";
    for (int i = 0; i < (int)results.size(); i++) {
        cout << "  " << left << setw(16) << results[i].name
             << right << fixed << setprecision(2)
             << setw(10) << results[i].avg_tat
             << setw(11) << results[i].avg_wait
             << setprecision(4) << setw(13) << results[i].throughput << "\n";
    }
    int best_tat_idx  = 0;
    int best_wait_idx = 0;
    for (int i = 1; i < (int)results.size(); i++) {
        if (results[i].avg_tat  < results[best_tat_idx].avg_tat)  best_tat_idx  = i;
        if (results[i].avg_wait < results[best_wait_idx].avg_wait) best_wait_idx = i;
    }
    cout << "\n  Best TAT  -> " << results[best_tat_idx].name
         << "    Best Wait -> " << results[best_wait_idx].name << "\n\n";
}
void send_to_io(Process* p, int t, vector<Process*>& io) {
    p->io_until   = t + p->io_len;
    p->slice_done = 0;
    io.push_back(p);
    for (int i = 0; i < (int)io.size(); i++) {
        for (int j = i + 1; j < (int)io.size(); j++) {
            if (io[j]->io_until < io[i]->io_until) swap(io[i], io[j]);
        }
    }
}
vector<Process*> make_copies(vector<Process> src) {
    vector<Process*> copies;
    for (int i = 0; i < (int)src.size(); i++) {
        copies.push_back(new Process(src[i]));
    }
    return copies;
}
void free_processes(vector<Process*>& v) {
    for (int i = 0; i < (int)v.size(); i++) {
        delete v[i];
    }
    v.clear();
}
vector<Process*> deque_to_vec(deque<Process*> dq) {
    vector<Process*> v;
    while (!dq.empty()) {
        v.push_back(dq.front());
        dq.pop_front();
    }
    return v;
}
vector<Process*> pq_to_vec(priority_queue<Process*, vector<Process*>, ShortestFirst> pq) {
    vector<Process*> v;
    while (!pq.empty()) {
        v.push_back(pq.top());
        pq.pop();
    }
    return v;
}
int earliest_event(vector<Process*> job, vector<Process*> io) {
    int earliest = INT_MAX;
    if (!job.empty()) {
        earliest = min(earliest, job[0]->arrival);
    }
    for (int i = 0; i < (int)io.size(); i++) {
        earliest = min(earliest, io[i]->io_until);
    }
    if (earliest == INT_MAX) return -1;
    return earliest;
}
Summary run_rr(vector<Process> src, int quantum, bool verbose) {
    cout << "\n  == Round Robin (quantum=" << quantum << ") - Preemptive ==\n\n";
    vector<Process*> all   = make_copies(src);
    vector<Process*> job   = all;   
    deque<Process*>  ready;         
    vector<Process*> io;            
    vector<Process*> done;          
    Process* cpu = nullptr;         
    int t = 0;                      
    int q = 0;                      
    while ((int)done.size() < (int)all.size()) {
        vector<Process*> leftover_job;
        for (int i = 0; i < (int)job.size(); i++) {
            if (job[i]->arrival <= t) {
                job[i]->ready_since = t;
                ready.push_back(job[i]);
            } else {
                leftover_job.push_back(job[i]);
            }
        }
        job = leftover_job;
        vector<Process*> leftover_io;
        for (int i = 0; i < (int)io.size(); i++) {
            if (io[i]->io_until <= t) {
                io[i]->ready_since = t;
                ready.push_back(io[i]);
            } else {
                leftover_io.push_back(io[i]);
            }
        }
        io = leftover_io;
        if (cpu == nullptr && !ready.empty()) {
            cpu = ready.front();
            ready.pop_front();
            q = 0;
            if (cpu->started_at == -1) cpu->started_at = t;
            cpu->wait_total += t - cpu->ready_since;
            if (verbose) show_queues(t, job, deque_to_vec(ready), io, cpu, "dispatch " + cpu->name);
        }
        if (cpu == nullptr) {
            int nxt = earliest_event(job, io);
            if (nxt == -1) break;
            t = nxt;
            continue;
        }
        cpu->remaining--;
        cpu->slice_done++;
        q++;
        t++;
        vector<Process*> leftover_job2;
        for (int i = 0; i < (int)job.size(); i++) {
            if (job[i]->arrival <= t) {
                job[i]->ready_since = t;
                ready.push_back(job[i]);
            } else {
                leftover_job2.push_back(job[i]);
            }
        }
        job = leftover_job2;
        vector<Process*> leftover_io2;
        for (int i = 0; i < (int)io.size(); i++) {
            if (io[i]->io_until <= t) {
                io[i]->ready_since = t;
                ready.push_back(io[i]);
            } else {
                leftover_io2.push_back(io[i]);
            }
        }
        io = leftover_io2;
        if (cpu->wants_io()) {
            if (verbose) show_queues(t, job, deque_to_vec(ready), io, cpu, cpu->name + " -> I/O");
            send_to_io(cpu, t, io);
            cpu = nullptr;
            q = 0;
        } else if (cpu->remaining == 0) {
            cpu->finished_at = t;
            if (verbose) show_queues(t, job, deque_to_vec(ready), io, nullptr, cpu->name + " finished");
            done.push_back(cpu);
            cpu = nullptr;
            q = 0;
        } else if (q == quantum) {
            if (verbose) show_queues(t, job, deque_to_vec(ready), io, cpu, cpu->name + " preempted");
            cpu->ready_since = t;
            ready.push_back(cpu);
            cpu = nullptr;
            q = 0;
        }
    }
    Summary s = print_metrics("Round Robin (q=" + to_string(quantum) + ")", done, t);
    free_processes(all);
    return s;
}
Summary run_sjf(vector<Process> src, bool verbose) {
    cout << "\n  == SJF (Shortest Job First) - Non-Preemptive ==\n\n";
    vector<Process*> all = make_copies(src);
    vector<Process*> job = all;
    priority_queue<Process*, vector<Process*>, ShortestFirst> ready;
    vector<Process*> io;
    vector<Process*> done;
    Process* cpu = nullptr;
    int t = 0;
    while ((int)done.size() < (int)all.size()) {
        vector<Process*> leftover_job;
        for (int i = 0; i < (int)job.size(); i++) {
            if (job[i]->arrival <= t) {
                job[i]->ready_since = t;
                ready.push(job[i]);
            } else {
                leftover_job.push_back(job[i]);
            }
        }
        job = leftover_job;
        vector<Process*> leftover_io;
        for (int i = 0; i < (int)io.size(); i++) {
            if (io[i]->io_until <= t) {
                io[i]->ready_since = t;
                ready.push(io[i]);
            } else {
                leftover_io.push_back(io[i]);
            }
        }
        io = leftover_io;
        if (cpu == nullptr && !ready.empty()) {
            cpu = ready.top();
            ready.pop();
            if (cpu->started_at == -1) cpu->started_at = t;
            cpu->wait_total += t - cpu->ready_since;
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, cpu,
                "dispatch " + cpu->name + " (rem=" + to_string(cpu->remaining) + ")");
        }
        if (cpu == nullptr) {
            int nxt = earliest_event(job, io);
            if (nxt == -1) break;
            t = nxt;
            continue;
        }
        cpu->remaining--;
        cpu->slice_done++;
        t++;
        vector<Process*> leftover_job2;
        for (int i = 0; i < (int)job.size(); i++) {
            if (job[i]->arrival <= t) {
                job[i]->ready_since = t;
                ready.push(job[i]);
            } else {
                leftover_job2.push_back(job[i]);
            }
        }
        job = leftover_job2;
        vector<Process*> leftover_io2;
        for (int i = 0; i < (int)io.size(); i++) {
            if (io[i]->io_until <= t) {
                io[i]->ready_since = t;
                ready.push(io[i]);
            } else {
                leftover_io2.push_back(io[i]);
            }
        }
        io = leftover_io2;
        if (cpu->wants_io()) {
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, cpu, cpu->name + " -> I/O");
            send_to_io(cpu, t, io);
            cpu = nullptr;
        } else if (cpu->remaining == 0) {
            cpu->finished_at = t;
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, nullptr, cpu->name + " finished");
            done.push_back(cpu);
            cpu = nullptr;
        }
    }
    Summary s = print_metrics("SJF", done, t);
    free_processes(all);
    return s;
}
Summary run_srtf(vector<Process> src, bool verbose) {
    cout << "\n  == SRTF (Shortest Remaining Time First) - Preemptive ==\n\n";
    vector<Process*> all = make_copies(src);
    vector<Process*> job = all;
    priority_queue<Process*, vector<Process*>, ShortestFirst> ready;
    vector<Process*> io;
    vector<Process*> done;
    Process* cpu = nullptr;
    int t = 0;
    while ((int)done.size() < (int)all.size()) {
        vector<Process*> leftover_job;
        for (int i = 0; i < (int)job.size(); i++) {
            if (job[i]->arrival <= t) {
                job[i]->ready_since = t;
                ready.push(job[i]);
            } else {
                leftover_job.push_back(job[i]);
            }
        }
        job = leftover_job;
        vector<Process*> leftover_io;
        for (int i = 0; i < (int)io.size(); i++) {
            if (io[i]->io_until <= t) {
                io[i]->ready_since = t;
                ready.push(io[i]);
            } else {
                leftover_io.push_back(io[i]);
            }
        }
        io = leftover_io;
        if (cpu != nullptr && !ready.empty() && ready.top()->remaining < cpu->remaining) {
            cpu->ready_since = t;
            ready.push(cpu);
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, cpu, cpu->name + " preempted");
            cpu = nullptr;
        }
        if (cpu == nullptr && !ready.empty()) {
            cpu = ready.top();
            ready.pop();
            if (cpu->started_at == -1) cpu->started_at = t;
            cpu->wait_total += t - cpu->ready_since;
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, cpu,
                "dispatch " + cpu->name + " (rem=" + to_string(cpu->remaining) + ")");
        }
        if (cpu == nullptr) {
            int nxt = earliest_event(job, io);
            if (nxt == -1) break;
            t = nxt;
            continue;
        }
        cpu->remaining--;
        cpu->slice_done++;
        t++;
        vector<Process*> leftover_job2;
        for (int i = 0; i < (int)job.size(); i++) {
            if (job[i]->arrival <= t) {
                job[i]->ready_since = t;
                ready.push(job[i]);
            } else {
                leftover_job2.push_back(job[i]);
            }
        }
        job = leftover_job2;
        vector<Process*> leftover_io2;
        for (int i = 0; i < (int)io.size(); i++) {
            if (io[i]->io_until <= t) {
                io[i]->ready_since = t;
                ready.push(io[i]);
            } else {
                leftover_io2.push_back(io[i]);
            }
        }
        io = leftover_io2;
        if (cpu->wants_io()) {
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, cpu, cpu->name + " -> I/O");
            send_to_io(cpu, t, io);
            cpu = nullptr;
        } else if (cpu->remaining == 0) {
            cpu->finished_at = t;
            if (verbose) show_queues(t, job, pq_to_vec(ready), io, nullptr, cpu->name + " finished");
            done.push_back(cpu);
            cpu = nullptr;
        } else {
            if (!ready.empty() && ready.top()->remaining < cpu->remaining) {
                cpu->ready_since = t;
                ready.push(cpu);
                if (verbose) show_queues(t, job, pq_to_vec(ready), io, cpu, cpu->name + " preempted");
                cpu = nullptr;
            }
        }
    }
    Summary s = print_metrics("SRTF", done, t);
    free_processes(all);
    return s;
}
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./sos <input.txt> [--algo rr|sjf|srtf|all] [--quantum N] [--quiet]\n";
        return 1;
    }
    string file    = argv[1];
    string algo    = "all";
    int    quantum = 3;
    bool   verbose = true;
    for (int i = 2; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--algo" && i + 1 < argc) {
            algo = argv[++i];
        } else if (arg == "--quantum" && i + 1 < argc) {
            quantum = stoi(argv[++i]);
        } else if (arg == "--quiet") {
            verbose = false;
        }
    }
    vector<Process> procs = load(file);
    cout << "\n  Processes\n  " << string(44, '-') << "\n";
    cout << "  " << left << setw(8) << "Name"
         << right << setw(8) << "Arrival"
         << setw(6) << "CPU"
         << setw(10) << "I/O every"
         << setw(8) << "I/O len" << "\n";
    cout << "  " << string(44, '-') << "\n";
    for (int i = 0; i < (int)procs.size(); i++) {
        cout << "  " << left << setw(8) << procs[i].name
             << right << setw(8) << procs[i].arrival
             << setw(6) << procs[i].total_cpu
             << setw(10) << procs[i].io_every
             << setw(8) << procs[i].io_len << "\n";
    }
    cout << "\n";
    vector<Summary> results;
    if (algo == "rr"   || algo == "all") results.push_back(run_rr(procs, quantum, verbose));
    if (algo == "sjf"  || algo == "all") results.push_back(run_sjf(procs, verbose));
    if (algo == "srtf" || algo == "all") results.push_back(run_srtf(procs, verbose));
    if (results.size() > 1) print_comparison(results);
    return 0;
}
