/*
 * parking_locator_modified.c
 * University Campus Parking Spot Locator  —  Menu-Driven Interactive Version
 *
 * MODIFICATION: Each demo test case now prints:
 *   1. A full Dijkstra diagram (ASCII art) showing ALL nodes with their
 *      shortest distances from the source, edge weights, and the
 *      highlighted shortest path to the best parking spot.
 *   2. All parking spots listed with node, status, and distance.
 *   3. The shortest distance to the result is printed clearly.
 *
 * Compile : gcc -o parking_locator_modified parking_locator_modified.c -lm
 * Run     : ./parking_locator_modified
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define MAX_NODES  50
#define MAX_SPOTS  200
#define MAX_NAME   32
#define INF        INT_MAX

typedef struct {
    int    id;
    char   lot_name[MAX_NAME];
    int    zone;
    int    is_occupied;
    double x, y;
    int    node_id;
} ParkingSpot;

typedef struct Edge {
    int          dest;
    int          weight;
    struct Edge *next;
} Edge;

typedef struct {
    char   name[MAX_NAME];
    double x, y;
    Edge  *head;
} GraphNode;

typedef struct {
    GraphNode nodes[MAX_NODES];
    int       num_nodes;
} Graph;

typedef struct { int node, dist; } HeapEntry;
typedef struct { HeapEntry data[MAX_NODES * 4]; int size; } MinHeap;
typedef struct { int items[MAX_NODES]; int front, rear; } BFSQueue;

typedef struct {
    int    spot_id;
    char   lot_name[MAX_NAME];
    int    zone;
    double distance;
    int    path[MAX_NODES];
    int    path_len;
} SearchResult;

ParkingSpot spots[MAX_SPOTS];
int         num_spots = 0;
Graph       campus;

void clear_input(void) { int c; while ((c = getchar()) != '\n' && c != EOF); }
void pause_prompt(void) { printf("\n  Press ENTER to return to menu..."); getchar(); }

const char *zone_name(int z) {
    switch (z) {
        case 1: return "Faculty";
        case 2: return "Student";
        case 3: return "Visitor";
        case 4: return "Disabled";
        default: return "General";
    }
}

/* Min-Heap */
void heap_push(MinHeap *h, int node, int dist) {
    int i = h->size++;
    h->data[i].node = node; h->data[i].dist = dist;
    while (i > 0) {
        int p = (i-1)/2;
        if (h->data[p].dist <= h->data[i].dist) break;
        HeapEntry t = h->data[p]; h->data[p] = h->data[i]; h->data[i] = t;
        i = p;
    }
}
HeapEntry heap_pop(MinHeap *h) {
    HeapEntry top = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    while (1) {
        int l = 2*i+1, r = 2*i+2, s = i;
        if (l < h->size && h->data[l].dist < h->data[s].dist) s = l;
        if (r < h->size && h->data[r].dist < h->data[s].dist) s = r;
        if (s == i) break;
        HeapEntry t = h->data[i]; h->data[i] = h->data[s]; h->data[s] = t;
        i = s;
    }
    return top;
}

/* Graph */
int graph_add_node(Graph *g, const char *name, double x, double y) {
    int id = g->num_nodes++;
    strncpy(g->nodes[id].name, name, MAX_NAME-1);
    g->nodes[id].x = x; g->nodes[id].y = y; g->nodes[id].head = NULL;
    return id;
}
void graph_add_edge(Graph *g, int u, int v, int w) {
    Edge *e1 = malloc(sizeof(Edge)); e1->dest=v; e1->weight=w; e1->next=g->nodes[u].head; g->nodes[u].head=e1;
    Edge *e2 = malloc(sizeof(Edge)); e2->dest=u; e2->weight=w; e2->next=g->nodes[v].head; g->nodes[v].head=e2;
}

/* Dijkstra */
void dijkstra(Graph *g, int src, int dist[], int prev[]) {
    MinHeap heap; heap.size = 0;
    for (int i=0; i<g->num_nodes; i++) { dist[i]=INF; prev[i]=-1; }
    dist[src] = 0;
    heap_push(&heap, src, 0);
    while (heap.size > 0) {
        HeapEntry cur = heap_pop(&heap);
        int u = cur.node;
        if (cur.dist > dist[u]) continue;
        for (Edge *e = g->nodes[u].head; e; e = e->next) {
            int v = e->dest, nd = dist[u] + e->weight;
            if (nd < dist[v]) { dist[v]=nd; prev[v]=u; heap_push(&heap,v,nd); }
        }
    }
}

int reconstruct_path(int prev[], int dest, int path[]) {
    int stack[MAX_NODES], top=0, cur=dest;
    while (cur != -1) { stack[top++]=cur; cur=prev[cur]; }
    for (int i=0; i<top; i++) path[i]=stack[top-1-i];
    return top;
}

/* BFS */
void bfs_search(Graph *g, int start, int max_dist, int dist_map[], int reachable[], int *count) {
    BFSQueue q; q.front=q.rear=0;
    int visited[MAX_NODES]={0};
    q.items[q.rear++]=start; visited[start]=1; *count=0;
    while (q.front != q.rear) {
        int u = q.items[q.front++];
        reachable[(*count)++]=u;
        for (Edge *e=g->nodes[u].head; e; e=e->next) {
            int v=e->dest;
            if (!visited[v] && dist_map[v]<=max_dist) { visited[v]=1; q.items[q.rear++]=v; }
        }
    }
}

/* Spot ops */
void add_spot(int id, const char *lot, int zone, int occ, double x, double y, int nid) {
    ParkingSpot *s = &spots[num_spots++];
    s->id=id; strncpy(s->lot_name,lot,MAX_NAME-1);
    s->zone=zone; s->is_occupied=occ; s->x=x; s->y=y; s->node_id=nid;
}
int spot_index(int id) {
    for (int i=0; i<num_spots; i++) if (spots[i].id==id) return i;
    return -1;
}

/* Core search */
SearchResult find_nearest_spot(int user_node, int zone, int radius, int verbose) {
    SearchResult result; result.spot_id=-1; result.distance=INF; result.path_len=0;
    int dist[MAX_NODES], prev[MAX_NODES];
    dijkstra(&campus, user_node, dist, prev);
    int reachable[MAX_NODES], count;
    bfs_search(&campus, user_node, radius, dist, reachable, &count);
    if (verbose) {
        printf("\n  [Dijkstra] Distances computed from: %s\n", campus.nodes[user_node].name);
        printf("  [BFS]      Reachable nodes within %d m: %d\n", radius, count);
        printf("  [Scan]     Checking %d registered spots...\n\n", num_spots);
    }
    for (int i=0; i<num_spots; i++) {
        ParkingSpot *s = &spots[i];
        if (s->is_occupied) continue;
        if (zone != 0 && s->zone != zone) continue;
        int ok=0;
        for (int j=0; j<count; j++) if (reachable[j]==s->node_id) { ok=1; break; }
        if (!ok) continue;
        double d = dist[s->node_id];
        if (verbose) printf("  [CANDIDATE] Spot %-4d | %-6s | %-8s | %4.0f m\n", s->id, s->lot_name, zone_name(s->zone), d);
        if (d < result.distance) {
            result.distance=d; result.spot_id=s->id;
            strncpy(result.lot_name,s->lot_name,MAX_NAME-1);
            result.zone=s->zone;
            result.path_len=reconstruct_path(prev, s->node_id, result.path);
        }
    }
    return result;
}

/* ═══════════════════ DIJKSTRA DIAGRAM (ASCII ART) ════════════════
 *
 * Campus layout (fixed node IDs from init_campus):
 *   0=Main Gate   1=Library    2=Eng Block   3=Sci Block  4=Admin
 *   5=Lot-A Entry 6=Lot-B Entry 7=Lot-C Entry 8=Lot-D Entry 9=Canteen
 *
 * Visual layout:
 *
 *   [Eng Block]---105---[Sci Block]
 *        |50                  |40
 *   [Library]---120-->[Lot-B]  [Lot-C]
 *        |105              |50
 *        |              [Eng Block]
 *        |90
 *   [Main Gate]--130--[Admin]--90--[Library]
 *        |60               |70
 *   [Lot-A]           [Lot-D]
 *
 * ═════════════════════════════════════════════════════════════════ */

static int on_path(int *path, int plen, int n) {
    for (int i=0; i<plen; i++) if (path[i]==n) return 1;
    return 0;
}
static int edge_on_path(int *path, int plen, int u, int v) {
    for (int i=0; i+1<plen; i++)
        if ((path[i]==u && path[i+1]==v)||(path[i]==v && path[i+1]==u)) return 1;
    return 0;
}
static int get_w(int u, int v) {
    for (Edge *e=campus.nodes[u].head; e; e=e->next) if (e->dest==v) return e->weight;
    return 0;
}

/* Print a node cell: 22 chars wide */
static void pnode(int nid, int src, int *dist, int *path, int plen, int dest) {
    const char *nm = campus.nodes[nid].name;
    char ds[10];
    if (dist[nid]==INF) snprintf(ds,sizeof(ds),"INF");
    else snprintf(ds,sizeof(ds),"%d",dist[nid]);

    char cell[32];
    if (nid==src)
        snprintf(cell,sizeof(cell),">>%.9s %5s<<",nm,ds);
    else if (nid==dest && dest>=0)
        snprintf(cell,sizeof(cell),"**%.9s %5s**",nm,ds);
    else if (on_path(path,plen,nid))
        snprintf(cell,sizeof(cell)," *%.9s %5s* ",nm,ds);
    else
        snprintf(cell,sizeof(cell),"  %.9s %5s  ",nm,ds);
    printf("[%-20s]",cell);
}

/* Print horizontal edge connector */
static void pedge_h(int u, int v, int *path, int plen) {
    int w = get_w(u,v);
    if (w==0) { printf("       "); return; }
    char wstr[16]; snprintf(wstr,sizeof(wstr),"%d",w);
    if (edge_on_path(path,plen,u,v))
        printf("==%s==",wstr);
    else
        printf("--%s--",wstr);
}

/* Print vertical edge label (w in string centered in 22-char cell) */
static void pedge_v_line(int u, int v, int *path, int plen) {
    int w = get_w(u,v);
    char s[32];
    if (edge_on_path(path,plen,u,v))
        snprintf(s,sizeof(s),"     ||%d||     ",w);
    else
        snprintf(s,sizeof(s),"      |%d|      ",w);
    printf("%-22s",s);
}

void print_dijkstra_diagram(int src, int *dist, int *prev,
                             int *path, int plen, int dest_node)
{
    printf("\n");
    printf("  +------------------------------------------------------------------+\n");
    printf("  |                     DIJKSTRA GRAPH DIAGRAM                      |\n");
    printf("  |  >>>NODE d<<<  = SOURCE   **NODE d**  = BEST SPOT (destination) |\n");
    printf("  |   *NODE d*     = on path  [NODE d]    = other node              |\n");
    printf("  |   ==w==        = path edge   --w--    = other edge              |\n");
    printf("  |  (d = shortest distance in metres from source)                  |\n");
    printf("  +------------------------------------------------------------------+\n\n");

    /* Row 1: Eng Block ---105--- Sci Block */
    printf("        ");
    pnode(2,src,dist,path,plen,dest_node);
    pedge_h(2,3,path,plen);
    pnode(3,src,dist,path,plen,dest_node);
    printf("\n");

    /* Vert: Eng->LotB(50),  Sci->LotC(40) */
    printf("        ");
    pedge_v_line(2,6,path,plen);
    printf("         ");
    pedge_v_line(3,7,path,plen);
    printf("\n");

    /* Row 2: Library ---105--- Eng Block ---50--- Lot-B Entry */
    printf("        ");
    pnode(1,src,dist,path,plen,dest_node);
    pedge_h(1,2,path,plen);
    pnode(2,src,dist,path,plen,dest_node);
    pedge_h(2,6,path,plen);
    pnode(6,src,dist,path,plen,dest_node);
    printf("\n");

    /* Also show Library---120---Lot-B and  Sci Block below */
    {
        int w_lb = get_w(1,6);
        int p_lb = edge_on_path(path,plen,1,6);
        printf("        (Library %s%dm%s Lot-B Entry)             ",
               p_lb?"==":"--", w_lb, p_lb?"==":"--");
        pnode(7,src,dist,path,plen,dest_node);
        printf("  <- Lot-C Entry\n");
    }

    /* Vert: Library->Gate(110), Library->Admin(90) */
    printf("        ");
    pedge_v_line(0,1,path,plen);
    pedge_v_line(1,4,path,plen);
    printf("\n");

    /* Row 3: Main Gate ---130--- Admin ---90--- Library */
    printf("        ");
    pnode(0,src,dist,path,plen,dest_node);
    pedge_h(0,4,path,plen);
    pnode(4,src,dist,path,plen,dest_node);
    pedge_h(1,4,path,plen);
    pnode(1,src,dist,path,plen,dest_node);
    printf("\n");

    /* Vert: Gate->LotA(60),  Admin->LotD(70) */
    printf("        ");
    pedge_v_line(0,5,path,plen);
    pedge_v_line(4,8,path,plen);
    printf("\n");

    /* Row 4: Lot-A Entry         Lot-D Entry */
    printf("        ");
    pnode(5,src,dist,path,plen,dest_node);
    printf("                      ");
    pnode(8,src,dist,path,plen,dest_node);
    printf("\n\n");

    /* ── Parking spots table ── */
    printf("  +-------+------+-------+----------+----------+---------+\n");
    printf("  | Node  |SpotID|  Lot  |   Zone   |  Status  | Dist(m) |\n");
    printf("  +-------+------+-------+----------+----------+---------+\n");
    for (int i=0; i<num_spots; i++) {
        ParkingSpot *s = &spots[i];
        int d = dist[s->node_id];
        char dstr[12];
        if (d==INF) snprintf(dstr,sizeof(dstr),"UNREACHBL");
        else        snprintf(dstr,sizeof(dstr),"%dm",d);
        int is_dest = (s->node_id == dest_node && dest_node>=0);
        printf("  |%-7s|%-6d|%-7s|%-10s|%-10s|%-9s|%s\n",
               campus.nodes[s->node_id].name,
               s->id, s->lot_name, zone_name(s->zone),
               s->is_occupied ? "OCCUPIED" : "FREE",
               dstr,
               is_dest ? " <<< BEST SPOT" : "");
    }
    printf("  +-------+------+-------+----------+----------+---------+\n\n");

    /* ── Distance table ── */
    printf("  DIJKSTRA DISTANCE TABLE  (source = %s)\n", campus.nodes[src].name);
    printf("  +------------------+----------+------------------+\n");
    printf("  | Node             | Dist (m) | Via              |\n");
    printf("  +------------------+----------+------------------+\n");
    for (int i=0; i<campus.num_nodes; i++) {
        char ds[12], via[MAX_NAME]="---";
        if (dist[i]==INF) snprintf(ds,sizeof(ds),"INF");
        else              snprintf(ds,sizeof(ds),"%d",dist[i]);
        if (prev[i]>=0) strncpy(via,campus.nodes[prev[i]].name,MAX_NAME-1);

        const char *tag =
            (i==src)                        ? " << SOURCE"      :
            (i==dest_node && dest_node>=0)  ? " << DESTINATION" :
            on_path(path,plen,i)            ? " << on path"     : "";

        printf("  | %-16s | %-8s | %-16s |%s\n",
               campus.nodes[i].name, ds, via, tag);
    }
    printf("  +------------------+----------+------------------+\n");

    /* ── Edge list ── */
    printf("\n  EDGES (weight | path-edge marked)\n");
    printf("  +------------------+------------------+--------+\n");
    printf("  | From             | To               | Weight |\n");
    printf("  +------------------+------------------+--------+\n");
    for (int u=0; u<campus.num_nodes; u++) {
        for (Edge *e=campus.nodes[u].head; e; e=e->next) {
            if (e->dest > u) {
                int on_p = edge_on_path(path,plen,u,e->dest);
                printf("  | %-16s | %-16s | %4dm  |%s\n",
                       campus.nodes[u].name, campus.nodes[e->dest].name,
                       e->weight, on_p?" << PATH EDGE":"");
            }
        }
    }
    printf("  +------------------+------------------+--------+\n\n");
}

/* ═══════════════════════════ Campus Init ══════════════════════════ */
void init_campus(void) {
    campus.num_nodes = 0; num_spots = 0;

    int gate  = graph_add_node(&campus, "Main Gate",    0,   0);
    int lib   = graph_add_node(&campus, "Library",    100,  50);
    int eng   = graph_add_node(&campus, "Eng Block",  200,  80);
    int sci   = graph_add_node(&campus, "Sci Block",  180, 180);
    int admin = graph_add_node(&campus, "Admin",       60, 120);
    int lotA  = graph_add_node(&campus, "Lot-A Entry", 50,  40);
    int lotB  = graph_add_node(&campus, "Lot-B Entry",210,  50);
    int lotC  = graph_add_node(&campus, "Lot-C Entry",170, 200);
    int lotD  = graph_add_node(&campus, "Lot-D Entry", 70, 150);
    int cant  = graph_add_node(&campus, "Canteen",    130, 130);
    (void)cant;

    graph_add_edge(&campus, gate,  lotA,  60);
    graph_add_edge(&campus, gate,  lib,  110);
    graph_add_edge(&campus, lib,   eng,  105);
    graph_add_edge(&campus, lib,   lotB, 120);
    graph_add_edge(&campus, eng,   lotB,  50);
    graph_add_edge(&campus, eng,   sci,  105);
    graph_add_edge(&campus, sci,   lotC,  40);
    graph_add_edge(&campus, admin, lotD,  70);
    graph_add_edge(&campus, gate,  admin,130);
    graph_add_edge(&campus, lib,   admin, 90);

    add_spot(101,"Lot-A",2,0,48, 38,lotA);
    add_spot(102,"Lot-A",2,1,50, 40,lotA);
    add_spot(103,"Lot-A",1,0,52, 42,lotA);
    add_spot(104,"Lot-A",4,0,46, 36,lotA);
    add_spot(201,"Lot-B",1,0,208,48,lotB);
    add_spot(202,"Lot-B",2,0,212,52,lotB);
    add_spot(203,"Lot-B",2,1,215,50,lotB);
    add_spot(204,"Lot-B",3,0,206,54,lotB);
    add_spot(301,"Lot-C",2,1,168,198,lotC);
    add_spot(302,"Lot-C",1,0,172,202,lotC);
    add_spot(303,"Lot-C",3,0,175,200,lotC);
    add_spot(401,"Lot-D",2,0,68, 148,lotD);
    add_spot(402,"Lot-D",4,0,72, 152,lotD);
    add_spot(403,"Lot-D",1,1,65, 150,lotD);
}

/* ═══════════════════════════ Menu functions ═══════════════════════ */
void print_banner(void) {
    printf("\n  +=========================================================+\n");
    printf("  |       UNIVERSITY CAMPUS PARKING SPOT LOCATOR           |\n");
    printf("  |        Data Structures & Algorithms - Project          |\n");
    printf("  +=========================================================+\n");
}

void print_main_menu(void) {
    print_banner();
    printf("\n  MAIN MENU\n  ---------\n");
    printf("  1. Find Nearest Available Parking Spot\n");
    printf("  2. View All Parking Spots & Status\n");
    printf("  3. Update Spot Occupancy  (Park / Leave)\n");
    printf("  4. View Campus Road Map\n");
    printf("  5. View Spots in a Specific Zone\n");
    printf("  6. Availability Summary by Lot\n");
    printf("  7. View Algorithm Process Flow\n");
    printf("  8. Run All 5 Demo Test Cases\n");
    printf("  9. Exit\n");
    printf("\n  Enter choice [1-9]: ");
}

void menu_find_spot(void) {
    int user_node, zone, radius;
    printf("\n  +--- FIND NEAREST PARKING SPOT ---+\n\n  Campus Locations:\n");
    for (int i=0; i<campus.num_nodes; i++) printf("    [%2d] %s\n",i,campus.nodes[i].name);
    printf("\n  Enter your current location number: ");
    if (scanf("%d",&user_node)!=1||user_node<0||user_node>=campus.num_nodes) {
        printf("  Invalid location.\n"); clear_input(); pause_prompt(); return;
    }
    clear_input();
    printf("\n  Zones: [0]Any [1]Faculty [2]Student [3]Visitor [4]Disabled\n  Enter your zone: ");
    if (scanf("%d",&zone)!=1||zone<0||zone>4) { printf("  Invalid zone.\n"); clear_input(); pause_prompt(); return; }
    clear_input();
    printf("\n  Enter search radius in metres (e.g. 200, 500): ");
    if (scanf("%d",&radius)!=1||radius<=0) { printf("  Invalid radius.\n"); clear_input(); pause_prompt(); return; }
    clear_input();

    printf("\n  Searching...\n  Location : %s\n  Zone     : %s\n  Radius   : %d m\n",
           campus.nodes[user_node].name, zone==0?"Any":zone_name(zone), radius);

    SearchResult r = find_nearest_spot(user_node, zone, radius, 1);

    int dist[MAX_NODES], prev[MAX_NODES];
    dijkstra(&campus, user_node, dist, prev);
    int dest_node = -1;
    if (r.spot_id>=0) { int idx=spot_index(r.spot_id); if (idx>=0) dest_node=spots[idx].node_id; }
    print_dijkstra_diagram(user_node, dist, prev, r.path, r.path_len, dest_node);

    printf("\n  =========== RESULT ===========\n");
    if (r.spot_id==-1) { printf("  No available spot found within %d m.\n", radius); }
    else {
        printf("  Spot ID   : %d\n  Lot       : %s\n  Zone      : %s\n  Distance  : %.0f m\n  Route     : ",
               r.spot_id, r.lot_name, zone_name(r.zone), r.distance);
        for (int i=0; i<r.path_len; i++) { printf("%s",campus.nodes[r.path[i]].name); if (i<r.path_len-1) printf(" --> "); }
        printf("\n  ==============================\n");
    }
    pause_prompt();
}

void menu_view_all_spots(void) {
    printf("\n  +--- ALL PARKING SPOTS ---+\n\n");
    printf("  %-5s %-7s %-10s %-10s %-16s\n","ID","Lot","Zone","Status","Nearest Node");
    printf("  %s\n","--------------------------------------------------");
    for (int i=0; i<num_spots; i++) {
        ParkingSpot *s=&spots[i];
        printf("  %-5d %-7s %-10s %-10s %-16s\n",s->id,s->lot_name,zone_name(s->zone),
               s->is_occupied?"Occupied":"FREE",campus.nodes[s->node_id].name);
    }
    int fc=0; for (int i=0; i<num_spots; i++) if (!spots[i].is_occupied) fc++;
    printf("\n  Total: %d | Free: %d | Occupied: %d\n",num_spots,fc,num_spots-fc);
    pause_prompt();
}

void menu_update_occupancy(void) {
    printf("\n  +--- UPDATE SPOT OCCUPANCY ---+\n\n");
    printf("  %-5s %-7s %-10s %-10s\n","ID","Lot","Zone","Status");
    for (int i=0; i<num_spots; i++) {
        ParkingSpot *s=&spots[i];
        printf("  %-5d %-7s %-10s %-10s\n",s->id,s->lot_name,zone_name(s->zone),s->is_occupied?"Occupied":"FREE");
    }
    int id; printf("\n  Enter Spot ID to toggle (park/leave): ");
    if (scanf("%d",&id)!=1) { clear_input(); pause_prompt(); return; }
    clear_input();
    int idx=spot_index(id);
    if (idx==-1) { printf("  Spot ID %d not found.\n",id); pause_prompt(); return; }
    spots[idx].is_occupied ^= 1;
    printf("\n  Spot %d [%s, %s] is now: %s\n",id,spots[idx].lot_name,zone_name(spots[idx].zone),
           spots[idx].is_occupied?"Occupied":"FREE");
    pause_prompt();
}

void menu_road_map(void) {
    printf("\n  +--- CAMPUS ROAD MAP (Adjacency List) ---+\n\n");
    for (int i=0; i<campus.num_nodes; i++) {
        printf("  [%2d] %-16s --> ",i,campus.nodes[i].name);
        Edge *e=campus.nodes[i].head;
        if (!e) { printf("(none)\n"); continue; }
        while (e) { printf("%s(%dm)",campus.nodes[e->dest].name,e->weight); e=e->next; if(e)printf(", "); }
        printf("\n");
    }
    pause_prompt();
}

void menu_spots_by_zone(void) {
    printf("\n  +--- SPOTS BY ZONE ---+\n\n  [1]Faculty [2]Student [3]Visitor [4]Disabled\n  Enter zone: ");
    int zone; if (scanf("%d",&zone)!=1||zone<1||zone>4) { printf("  Invalid.\n"); clear_input(); pause_prompt(); return; }
    clear_input();
    printf("\n  Zone: %s\n  %-5s %-7s %-10s\n",zone_name(zone),"ID","Lot","Status");
    int found=0;
    for (int i=0; i<num_spots; i++) if (spots[i].zone==zone) {
        printf("  %-5d %-7s %-10s\n",spots[i].id,spots[i].lot_name,spots[i].is_occupied?"Occupied":"FREE"); found++;
    }
    if (!found) printf("  No spots registered in this zone.\n");
    pause_prompt();
}

void menu_availability_summary(void) {
    printf("\n  +--- AVAILABILITY SUMMARY BY LOT ---+\n\n");
    const char *lots[]={"Lot-A","Lot-B","Lot-C","Lot-D"}; int nlots=4;
    printf("  %-8s %-6s %-8s %-8s %-10s\n","Lot","Total","Free","Occupied","Fill %");
    for (int l=0; l<nlots; l++) {
        int total=0,free=0;
        for (int i=0; i<num_spots; i++) if (strcmp(spots[i].lot_name,lots[l])==0) { total++; if (!spots[i].is_occupied) free++; }
        printf("  %-8s %-6d %-8d %-8d %d%%\n",lots[l],total,free,total-free,total>0?(int)(100.0*(total-free)/total):0);
    }
    pause_prompt();
}

void menu_process_flow(void) {
    printf("\n  ALGORITHM PROCESS FLOW\n  ======================\n\n");
    printf("  STEP 1 - INPUT: location, zone, radius\n");
    printf("  STEP 2 - DIJKSTRA O((V+E)logV): compute dist[], prev[] from source\n");
    printf("  STEP 3 - BFS O(V+E): filter nodes within radius\n");
    printf("  STEP 4 - SCAN O(S): find best free spot in zone that is reachable\n");
    printf("  STEP 5 - PATH RECONSTRUCTION O(V): trace back via prev[]\n");
    printf("  STEP 6 - OUTPUT: spot ID, distance, route\n\n");
    printf("  COMPLEXITY: Dijkstra O((V+E)logV) | BFS O(V+E) | Scan O(S)\n");
    pause_prompt();
}

/* ── 8. Demo Test Cases — NOW WITH DIJKSTRA DIAGRAMS ── */

void run_demo_test(int tc, const char *loc, int node, int zone, int radius) {
    printf("\n");
    printf("  +===========================================================+\n");
    printf("  |  TEST CASE %d                                              |\n", tc);
    printf("  +===========================================================+\n");
    printf("  Source   : %s (node %d)\n", loc, node);
    printf("  Zone     : %s\n", zone==0?"Any":zone_name(zone));
    printf("  Radius   : %d m\n", radius);
    printf("  -----------------------------------------------------------\n");

    /* Run Dijkstra to get dist[] and prev[] */
    int dist[MAX_NODES], prev[MAX_NODES];
    dijkstra(&campus, node, dist, prev);

    /* Run full search */
    SearchResult r = find_nearest_spot(node, zone, radius, 0);

    /* Get destination node */
    int dest_node = -1;
    if (r.spot_id>=0) {
        int idx = spot_index(r.spot_id);
        if (idx>=0) dest_node = spots[idx].node_id;
    }

    /* Print Dijkstra diagram */
    print_dijkstra_diagram(node, dist, prev, r.path, r.path_len, dest_node);

    /* Print result */
    printf("  ==================== RESULT ====================\n");
    if (r.spot_id==-1) {
        printf("  No available spot found within %d m.\n", radius);
        printf("  Tip: Increase radius or choose zone = Any.\n");
    } else {
        printf("  Spot ID          : %d\n",     r.spot_id);
        printf("  Lot              : %s\n",     r.lot_name);
        printf("  Zone             : %s\n",     zone_name(r.zone));
        printf("  Shortest Distance: %.0f m\n", r.distance);
        printf("  Route            : ");
        for (int i=0; i<r.path_len; i++) {
            printf("%s", campus.nodes[r.path[i]].name);
            if (i<r.path_len-1) printf(" --> ");
        }
        printf("\n");
    }
    printf("  ================================================\n");
}

void menu_demo_tests(void) {
    printf("\n  +===== 5 DEMO TEST CASES WITH DIJKSTRA DIAGRAMS =====+\n");
    run_demo_test(1, "Main Gate",  0, 2, 500);
    run_demo_test(2, "Library",    1, 1, 150);
    run_demo_test(3, "Eng Block",  2, 3, 300);
    run_demo_test(4, "Admin",      4, 4, 200);
    run_demo_test(5, "Sci Block",  3, 0,  30);
    pause_prompt();
}

/* ═══════════════════════════ Main ═════════════════════════════════ */
int main(void) {
    init_campus();
    int choice;
    while (1) {
        print_main_menu();
        if (scanf("%d",&choice)!=1) { clear_input(); printf("  Please enter 1-9.\n"); continue; }
        clear_input();
        switch (choice) {
            case 1: menu_find_spot();            break;
            case 2: menu_view_all_spots();       break;
            case 3: menu_update_occupancy();     break;
            case 4: menu_road_map();             break;
            case 5: menu_spots_by_zone();        break;
            case 6: menu_availability_summary(); break;
            case 7: menu_process_flow();         break;
            case 8: menu_demo_tests();           break;
            case 9: printf("\n  Thank you. Goodbye!\n\n"); return 0;
            default: printf("\n  Invalid choice. Enter 1-9.\n");
        }
    }
}
