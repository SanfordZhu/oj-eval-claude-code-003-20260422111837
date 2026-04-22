#include <bits/stdc++.h>
using namespace std;

struct ProblemState {
    int wrong_before=0; // wrong attempts before first AC (or before freeze)
    bool solved=false;
    int solve_time=0; // time of first AC
    int submissions_after_freeze=0; // total submissions during freeze (for display y)
    int wrong_freeze_before_ac=0; // wrong attempts during freeze before first AC (for penalty)
    bool frozen=false; // whether this problem is currently frozen for the team
};

struct Submission { // keep minimal fields
    char prob;
    string status;
    int time;
};

struct Team {
    string name;
    // problem states indexed 0..M-1
    vector<ProblemState> probs;
    // submissions record (all, including after freeze) for QUERY_SUBMISSION
    vector<Submission> subs;

    // scoring caches for current visible scoreboard (ignores frozen problems)
    int solved_visible=0;
    long long penalty_visible=0;
    vector<int> solve_times_visible; // times of solved problems (visible), sorted desc for tie-break
    int last_flushed_rank=0; // rank after last FLUSH
};

struct Contest {
    bool started=false;
    int duration=0;
    int M=0; // number of problems (A..)
    map<string,int> team_index; // name -> idx
    vector<Team> teams;

    bool frozen=false; // scoreboard frozen state
};

static Contest contest;

static bool is_ac(const string &s){ return s=="Accepted"; }
static bool is_wrong(const string &s){ return s=="Wrong_Answer" || s=="Runtime_Error" || s=="Time_Limit_Exceed"; }

// recompute visible scoreboard data (ignoring frozen problems)
static void recompute_visible() {
    for (auto &t: contest.teams){
        t.solved_visible=0; t.penalty_visible=0; t.solve_times_visible.clear();
        for (int i=0;i<contest.M;i++){
            const auto &ps=t.probs[i];
            if (ps.solved && !ps.frozen){
                t.solved_visible++;
                t.penalty_visible += 20LL*ps.wrong_before + ps.solve_time;
                t.solve_times_visible.push_back(ps.solve_time);
            }
        }
        sort(t.solve_times_visible.begin(), t.solve_times_visible.end(), greater<int>());
    }
}

// ranking comparator
static bool rank_cmp(const Team* a,const Team* b){
    if (a->solved_visible!=b->solved_visible) return a->solved_visible>b->solved_visible;
    if (a->penalty_visible!=b->penalty_visible) return a->penalty_visible<b->penalty_visible;
    // compare solve times (max smaller ranks higher); we stored descending list
    int na=a->solve_times_visible.size(), nb=b->solve_times_visible.size();
    int n=max(na,nb);
    for (int i=0;i<n;i++){
        int va = (i<na? a->solve_times_visible[i] : INT_MIN);
        int vb = (i<nb? b->solve_times_visible[i] : INT_MIN);
        if (va!=vb) return va<vb; // smaller max time ranks higher
    }
    return a->name < b->name;
}

static vector<Team*> current_ranking(){
    vector<Team*> arr;
    arr.reserve(contest.teams.size());
    for (auto &t: contest.teams) arr.push_back(&t);
    sort(arr.begin(), arr.end(), rank_cmp);
    for (size_t i=0;i<arr.size();i++) arr[i]->last_flushed_rank = (int)i+1;
    return arr;
}

static void print_scoreboard(const vector<Team*>& rank){
    // Output: team_name ranking solved_count total_penalty A B C ...
    for (auto *t: rank){
        cout << t->name << ' ' << t->last_flushed_rank << ' ' << t->solved_visible << ' ' << t->penalty_visible;
        for (int i=0;i<contest.M;i++){
            const auto &ps=t->probs[i];
            if (ps.frozen){
                // show -x/y but if x==0 show 0/y
                if (ps.wrong_before==0) cout << ' ' << "0/" << ps.submissions_after_freeze;
                else cout << ' ' << '-' << ps.wrong_before << '/' << ps.submissions_after_freeze;
            }else{
                if (ps.solved){
                    if (ps.wrong_before==0) cout << ' ' << '+'; else cout << ' ' << '+' << ps.wrong_before;
                }else{
                    if (ps.wrong_before==0) cout << ' ' << '.'; else cout << ' ' << '-' << ps.wrong_before;
                }
            }
        }
        cout << '\n';
    }
}

int main(){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    string cmd;
    while (cin>>cmd){
        if (cmd=="ADDTEAM"){
            string name; cin>>name;
            if (contest.started){
                cout << "[Error]Add failed: competition has started.\n";
            }else{
                if (contest.team_index.count(name)){
                    cout << "[Error]Add failed: duplicated team name.\n";
                }else{
                    Team t; t.name=name; contest.team_index[name]=(int)contest.teams.size(); contest.teams.push_back(move(t));
                    cout << "[Info]Add successfully.\n";
                }
            }
        }else if (cmd=="START"){
            string tmp; cin>>tmp; // DURATION
            int duration; cin>>duration; cin>>tmp; // PROBLEM
            int pc; cin>>pc;
            if (contest.started){
                cout << "[Error]Start failed: competition has started.\n";
            }else{
                contest.started=true; contest.duration=duration; contest.M=pc;
                for (auto &t: contest.teams) t.probs.assign(pc, ProblemState());
                cout << "[Info]Competition starts.\n";
            }
        }else if (cmd=="SUBMIT"){
            string prob; cin>>prob; // [problem_name]
            string tmp; cin>>tmp; // BY
            string team_name; cin>>team_name; cin>>tmp; // WITH
            string status; cin>>status; cin>>tmp; // AT
            int time; cin>>time;
            int ti = contest.team_index[team_name];
            Team &t = contest.teams[ti];
            char pid = prob[0]; int idx = pid - 'A';
            // record submission
            t.subs.push_back({pid,status,time});
            auto &ps = t.probs[idx];
            if (!contest.frozen){
                // normal update
                if (!ps.solved){
                    if (is_ac(status)){
                        ps.solved=true; ps.solve_time=time;
                    }else if (is_wrong(status)){
                        ps.wrong_before++;
                    }
                } // else solved already -> further submissions ignored
            }else{
                // frozen behavior: if this problem was not solved before freeze and not yet solved -> count into frozen bucket
                if (!ps.solved){
                    ps.frozen = true;
                    ps.submissions_after_freeze++;
                    if (is_ac(status)){
                        if (ps.solve_time==0) ps.solve_time=time; // first AC time during freeze
                    }else if (is_wrong(status)){
                        if (ps.solve_time==0) ps.wrong_freeze_before_ac++; // only before first AC during freeze
                    }
                }
            }
        }else if (cmd=="FLUSH"){
            cout << "[Info]Flush scoreboard.\n";
            recompute_visible();
            auto rank=current_ranking();
            // no scoreboard print here per spec; only at SCROLL
        }else if (cmd=="FREEZE"){
            if (contest.frozen){
                cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            }else{
                contest.frozen=true;
                // Mark frozen for problems that are unsolved and had submissions after freeze — per spec: problems with at least one submission after freezing enter frozen state.
                // We cannot know here; we will set frozen=true upon first post-freeze submission in SUBMIT.
                cout << "[Info]Freeze scoreboard.\n";
            }
        }else if (cmd=="SCROLL"){
            if (!contest.frozen){
                cout << "[Error]Scroll failed: scoreboard has not been frozen.";
            }else{
                cout << "[Info]Scroll scoreboard.\n";
                // Before scrolling, flush then print scoreboard
                recompute_visible();
                auto rank=current_ranking();
                print_scoreboard(rank);

                // Perform scrolling: repeatedly pick lowest-ranked team with frozen problems, unfreeze the smallest problem id, recalc, output rank changes when they happen
                // Helper to find next team and problem
                auto has_frozen = [&](Team* t)->bool{
                    for (int i=0;i<contest.M;i++) if (t->probs[i].frozen) return true; return false;
                };
                auto smallest_frozen = [&](Team* t)->int{
                    for (int i=0;i<contest.M;i++) if (t->probs[i].frozen) return i; return -1;
                };

                while (true){
                    // recompute rank order to find lowest-ranked team with frozen problems
                    recompute_visible();
                    rank=current_ranking();
                    Team* target=nullptr; int targetProb=-1;
                    for (int i=(int)rank.size()-1;i>=0;i--){
                        Team* t = rank[i];
                        if (has_frozen(t)){ target=t; targetProb=smallest_frozen(t); break; }
                    }
                    if (!target) break; // no frozen problems left

                    // Unfreeze that problem
                    auto &ps = target->probs[targetProb];
                    // When unfreezing: apply wrong_freeze_before_ac and accept
                    if (!ps.solved && ps.solve_time>0){
                        ps.solved=true;
                    }
                    // add wrong attempts during freeze that were before first AC to wrong_before
                    ps.wrong_before += ps.wrong_freeze_before_ac;
                    ps.wrong_freeze_before_ac = 0;
                    ps.submissions_after_freeze = 0;
                    ps.frozen=false;

                    // After unfreeze, recompute and see if ranking changed; output one line per change event
                    recompute_visible();
                    auto newrank=current_ranking();
                    // find target's new position and previous position to detect real change
                    int newpos=-1; for (int i=0;i<(int)newrank.size();i++) if (newrank[i]==target) { newpos=i; break; }
                    // compute old ranking before this unfreeze by recomputing without the change: we approximated earlier rank variable; capture previous position from that
                    int oldpos=-1; for (int i=0;i<(int)rank.size();i++) if (rank[i]==target){ oldpos=i; break; }
                    if (newpos!=-1 && oldpos!=-1 && newpos<oldpos){
                        Team* replaced = newrank[newpos+1];
                        cout << target->name << ' ' << replaced->name << ' ' << target->solved_visible << ' ' << target->penalty_visible << '\n';
                    }
                }

                // After scrolling ends, print final scoreboard and lift frozen state
                recompute_visible();
                rank=current_ranking();
                print_scoreboard(rank);
                contest.frozen=false; // lift frozen state
            }
        }else if (cmd=="QUERY_RANKING"){
            string team; cin>>team;
            auto it = contest.team_index.find(team);
            if (it==contest.team_index.end()){
                cout << "[Error]Query ranking failed: cannot find the team.\n";
            }else{
                cout << "[Info]Complete query ranking.\n";
                if (contest.frozen){
                    cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
                }
                // ranking after last FLUSH
                cout << team << " NOW AT RANKING " << contest.teams[it->second].last_flushed_rank << '\n';
            }
        }else if (cmd=="QUERY_SUBMISSION"){
            string team; cin>>team; string tmp; cin>>tmp; // WHERE
            string probEq; cin>>probEq; // PROBLEM=...
            string probSpec = probEq.substr(strlen("PROBLEM="));
            cin>>tmp; // AND
            string statusEq; cin>>statusEq; // STATUS=...
            string statusSpec = statusEq.substr(strlen("STATUS="));

            auto it = contest.team_index.find(team);
            if (it==contest.team_index.end()){
                cout << "[Error]Query submission failed: cannot find the team.\n";
            }else{
                cout << "[Info]Complete query submission.\n";
                Team &t = contest.teams[it->second];
                bool any=false; Submission last;
                for (const auto &s: t.subs){
                    if ((probSpec=="ALL" || probSpec[0]==s.prob) && (statusSpec=="ALL" || statusSpec==s.status)){
                        any=true; last=s;
                    }
                }
                if (!any){
                    cout << "Cannot find any submission.\n";
                }else{
                    cout << t.name << ' ' << last.prob << ' ' << last.status << ' ' << last.time << '\n';
                }
            }
        }else if (cmd=="END"){
            cout << "[Info]Competition ends.\n";
            break;
        }
    }
    return 0;
}
