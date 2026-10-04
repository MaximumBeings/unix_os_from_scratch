/* Chapter 55: the Raft consensus algorithm (Ongaro and Ousterhout, "In Search of an Understandable Consensus Algorithm", 2014): a group of servers keeps one replicated log in the same order everywhere, survives crashes and lost, delayed,
 * duplicated and reordered messages, and never lets two servers disagree about a committed entry, as long as a majority can talk to each other.
 *
 * THE NODE here is a pure state machine: it does no I/O and has no clock of its own. The caller (the simulator of 055_sim.c, or a real network layer) hands it ticks and incoming messages and a random number, and collects the messages it wants sent.
 *   persistent state (must survive a crash, saved BEFORE any reply leaves the node): currentTerm, votedFor, the log                       -> raft_disk_t, raft_t.dirty says "save me"
 *   volatile state: role, commitIndex, lastApplied, election and heartbeat timers, and for a leader nextIndex[] and matchIndex[]
 *   RULES, as in the paper: (1) a term number only ever grows; a message from a higher term turns any node into a follower of it. (2) A node grants at most one vote per term, and only to a candidate whose log is at least as up to date as its own
 *   (higher last term, or equal last term and at least as long). (3) A candidate with votes from a majority becomes leader. (4) A leader appends client commands, sends them with the index and term of the entry just before them (the consistency
 *   check); a follower that does not hold that entry refuses and the leader backs up. A follower that finds a conflicting entry deletes it and everything after it. (5) A leader may advance commitIndex only to an index of its OWN term that a majority
 *   holds (entries of earlier terms become committed indirectly) -- the rule of the paper's Figure 8. A new leader appends a no-op (command 0) so that it can commit at once.
 * NOT covered: cluster membership changes, log compaction and snapshots, read-only queries, and a real network or disk (the simulator models the disk as an atomic save and the network as a lossy queue). Not a production Raft. */
#ifndef RAFT_H
#define RAFT_H
#include <stdint.h>

#define RAFT_MAX_NODES 7
#define RAFT_LOG_MAX 64
#define RAFT_MAX_ENT 4        /* entries carried by one AppendEntries */
#define RAFT_ELECT_MIN 10     /* election timeout, in ticks: RAFT_ELECT_MIN + a random number below RAFT_ELECT_RANGE */
#define RAFT_ELECT_RANGE 10
#define RAFT_HEARTBEAT 3
enum { RAFT_FOLLOWER = 0, RAFT_CANDIDATE = 1, RAFT_LEADER = 2 };
enum { RAFT_MSG_RV = 1, RAFT_MSG_RVR = 2, RAFT_MSG_AE = 3, RAFT_MSG_AER = 4 };
enum { RAFT_OK = 0, RAFT_ERR_NOT_LEADER = -1, RAFT_ERR_FULL = -2 };
/* deliberate bugs, switched on by the tests and by one part of the kernel demo, to show the simulator finding them: 0 = none */
enum { RAFT_BUG_NONE = 0, RAFT_BUG_NO_LOG_CHECK_IN_VOTE = 1, RAFT_BUG_COMMIT_OLD_TERM = 2, RAFT_BUG_VOTE_NOT_PERSISTED = 3, RAFT_BUG_NO_CONSISTENCY_CHECK = 4, RAFT_BUG_MINORITY_COMMIT = 5, RAFT_BUG_COMMIT_REGRESS = 6 };
extern int raft_bug;

typedef struct { uint32_t term, cmd; } raft_ent_t;
typedef struct { uint32_t term; int32_t voted_for; uint32_t log_len; raft_ent_t log[RAFT_LOG_MAX]; } raft_disk_t; /* log index i (1-based) is log[i - 1] */
typedef struct {
    uint8_t type, from, to; uint32_t term;
    uint32_t last_idx, last_term;                      /* RequestVote: the candidate's last log index and term */
    uint8_t granted;                                   /* RequestVote reply */
    uint32_t prev_idx, prev_term, commit, nent; raft_ent_t ent[RAFT_MAX_ENT]; /* AppendEntries */
    uint8_t success; uint32_t match;                   /* AppendEntries reply: on success the highest index now known to match */
} raft_msg_t;
typedef struct {
    int id, n, role, leader; raft_disk_t d; int dirty;
    uint32_t commit, applied, elapsed, timeout, hb, votes; uint32_t next[RAFT_MAX_NODES], match[RAFT_MAX_NODES];
    uint32_t sm_count, sm_hash; /* the replicated state machine: the number of client commands applied and a hash chain over them (no-ops are not counted) */
} raft_t;

void raft_init(raft_t *r, int id, int n, const raft_disk_t *disk, uint32_t rnd); /* disk may be 0 (a new node); a restarted node passes what it saved */
int raft_tick(raft_t *r, uint32_t rnd, raft_msg_t *out);                          /* returns the number of messages to send (at most n) */
int raft_recv(raft_t *r, const raft_msg_t *m, uint32_t rnd, raft_msg_t *out);
int raft_propose(raft_t *r, uint32_t cmd, uint32_t *index);                       /* RAFT_OK and the entry's index, or an error; cmd 0 is reserved for the no-op */
#endif
