template <typename visit_func, typename backtrack_func, typename traverse_func>
DFS<visit_func, backtrack_func, traverse_func>::DFS(const LagGraph & graph_in, const LagGraph::vertex start, visit_func visit_in, backtrack_func backtrack_in, traverse_func traverse_in, bool reverse)
  : visit(visit_in), backtrack(backtrack_in), traverse(traverse_in), graph(graph_in), visited(graph_in.vertex_count(), false)
{
  this->dfs_rec(start, LagGraph::no_vertex, reverse);
}

template <typename visit_func, typename backtrack_func, typename traverse_func>
void
DFS<visit_func, backtrack_func, traverse_func>::dfs_rec(vertex v, vertex from, bool reverse)
{
  this->visited[v] = true;
  bool keep_going = this->visit(v, from);

  if (keep_going) {
    if (!reverse) {
      auto container = this->graph.neighbors(v);
      for (auto edge : container) {
        this->traverse(v, edge.t, edge.lag);
        if (!this->visited[edge.t]) {
          this->dfs_rec(edge.t, v, reverse);
        }
      }
    } else {
      auto container = this->graph.reverse_neighbors(v);
      for (auto edge : container) {
        this->traverse(v, edge.t, edge.lag);
        if (!this->visited[edge.t]) {
          this->dfs_rec(edge.t, v, reverse);
        }
      }
    }
  }

  this->backtrack(v);
}
