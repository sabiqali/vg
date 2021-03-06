#ifndef VG_SURJECTOR_HPP_INCLUDED
#define VG_SURJECTOR_HPP_INCLUDED

/** \file
 *
 *  A class to hold surjection algorithms that do lossy realignment restricted to paths in the graph
 */

#include <set>

#include "alignment.hpp"
#include "aligner.hpp"
#include "vg.hpp"
#include "translator.hpp"
#include "utility.hpp"
#include <vg/vg.pb.h>
#include "multipath_alignment_graph.hpp"
#include "memoizing_graph.hpp"
#include "split_strand_graph.hpp"

#include "algorithms/topological_sort.hpp"

#include "bdsg/hash_graph.hpp"

namespace vg {

using namespace std;

    class Surjector : public AlignerClient {
    public:
        
        Surjector(const PathPositionHandleGraph* graph);
        
        /// Extract the portions of an alignment that are on a chosen set of paths and try to
        /// align realign the portions that are off of the chosen paths to the intervening
        /// path segments to obtain an alignment that is fully restricted to the paths.
        ///
        /// Also returns the path name, position, and strand of the new alignment.
        ///
        /// Optionally either allow softclips so that the alignment has a nonnegative score on
        /// the path or require the full-length alignment, possibly creating a negative score.
        ///
        /// Also optionally leaves deletions against the reference path in the final alignment
        /// (useful for splicing).
        Alignment surject(const Alignment& source,
                          const unordered_set<path_handle_t>& paths,
                          string& path_name_out,
                          int64_t& path_pos_out,
                          bool& path_rev_out,
                          bool allow_negative_scores = false,
                          bool preserve_deletions = false) const;
                          
        /// Extract the portions of an alignment that are on a chosen set of
        /// paths and try to align realign the portions that are off of the
        /// chosen paths to the intervening path segments to obtain an
        /// alignment that is fully restricted to the paths.
        ///
        /// Replaces the alignment's refpos with the path name, position, and
        /// strand the alignment has been surjected to.
        ///
        /// Optionally either allow softclips so that the alignment has a
        /// nonnegative score on the path or require the full-length alignment,
        /// possibly creating a negative score.
        ///
        /// Also optionally leaves deletions against the reference path in the final
        /// alignment (useful for splicing).
        Alignment surject(const Alignment& source,
                          const unordered_set<path_handle_t>& paths,
                          bool allow_negative_scores = false,
                          bool preserve_deletions = false) const;
        
        /// Same semantics as with alignments except that connections are always
        /// preserved as splices. The output consists of a multipath alignment with
        /// a single path, separated by splices (either from large deletions or from
        /// connections)
        multipath_alignment_t surject(const multipath_alignment_t& source,
                                      const unordered_set<path_handle_t>& paths,
                                      string& path_name_out, int64_t& path_pos_out,
                                      bool& path_rev_out,
                                      bool allow_negative_scores = false,
                                      bool preserve_deletions = false) const;
        
        /// a local type that represents a read interval matched to a portion of the alignment path
        using path_chunk_t = pair<pair<string::const_iterator, string::const_iterator>, Path>;
        
        /// the minimum length deletion that the spliced algorithm will interpret as a splice event
        int64_t min_splice_length = 20;
        
        int64_t dominated_path_chunk_diff = 10;
        
    protected:
        
        void surject_internal(const Alignment* source_aln, const multipath_alignment_t* source_mp_aln,
                              Alignment* aln_out, multipath_alignment_t* mp_aln_out,
                              const unordered_set<path_handle_t>& paths,
                              string& path_name_out, int64_t& path_pos_out, bool& path_rev_out,
                              bool allow_negative_scores, bool preserve_deletions) const;
        
        Alignment
        realigning_surject(const PathPositionHandleGraph* graph, const Alignment& source,
                           const path_handle_t& path_handle, const vector<path_chunk_t>& path_chunks,
                           pair<step_handle_t, step_handle_t>& path_range_out,
                           bool allow_negative_scores,
                           bool preserve_N_alignments = false,
                           bool preserve_tail_indel_anchors = false) const;
        
        multipath_alignment_t
        spliced_surject(const PathPositionHandleGraph* path_position_graph,
                        const string& src_sequence, const string& src_quality,
                        const int32_t src_mapping_quality,
                        const path_handle_t& path_handle, vector<path_chunk_t>& path_chunks,
                        vector<pair<step_handle_t, step_handle_t>>& ref_chunks,
                        vector<tuple<size_t, size_t, int32_t>>& connections,
                        pair<step_handle_t, step_handle_t>& path_range_out,
                        bool allow_negative_scores, bool deletions_as_splices) const;
        
        ///////////////////////
        // Support methods for the realigning surject algorithm
        ///////////////////////
        
        /// get the chunks of the alignment path that follow the given reference paths
        unordered_map<path_handle_t, pair<vector<path_chunk_t>, vector<pair<step_handle_t, step_handle_t>>>>
        extract_overlapping_paths(const PathPositionHandleGraph* graph, const Alignment& source,
                                  const unordered_set<path_handle_t>& surjection_paths) const;
        
        /// same semantics except for a multipath alignment
        unordered_map<path_handle_t, pair<vector<path_chunk_t>, vector<pair<step_handle_t, step_handle_t>>>>
        extract_overlapping_paths(const PathPositionHandleGraph* graph,
                                  const multipath_alignment_t& source,
                                  const unordered_set<path_handle_t>& surjection_paths,
                                  unordered_map<path_handle_t, vector<tuple<size_t, size_t, int32_t>>>& connections_out) const;
        
        /// remove any path chunks and corresponding ref chunks that are identical to a longer
        /// path chunk over the region where they overlap
        void filter_redundant_path_chunks(vector<path_chunk_t>& path_chunks,
                                          vector<pair<step_handle_t, step_handle_t>>& ref_chunks,
                                          vector<tuple<size_t, size_t, int32_t>>& connections) const;
        
        /// compute the widest interval of path positions that the realigned sequence could align to
        pair<size_t, size_t>
        compute_path_interval(const PathPositionHandleGraph* graph, const Alignment& source, path_handle_t path_handle,
                              const vector<path_chunk_t>& path_chunks) const;
        
        /// make a linear graph that corresponds to a path interval, possibly duplicating nodes in case of cycles
        unordered_map<id_t, pair<id_t, bool>>
        extract_linearized_path_graph(const PathPositionHandleGraph* graph, MutableHandleGraph* into,
                                      path_handle_t path_handle, size_t first, size_t last) const;
        
        /// use the graph position bounds and the path range bounds to assign a path position to a surjected read
        void set_path_position(const PathPositionHandleGraph* graph, const pos_t& init_surj_pos,
                               const pos_t& final_surj_pos,
                               const step_handle_t& range_begin, const step_handle_t& range_end,
                               string& path_name_out, int64_t& path_pos_out, bool& path_rev_out) const;
        
        ///////////////////////
        // Support methods for the spliced surject algorithm
        ///////////////////////
        
        /// reverses an adjacency list
        vector<vector<size_t>> reverse_adjacencies(const vector<vector<size_t>>& adj) const;
        
        /// returns a vector assignming each node to a connectd component, requires both the forward and reverse adjacency
        /// lists. optionally also returns the total number of components
        vector<size_t> connected_components(const vector<vector<size_t>>& adj,
                                            const vector<vector<size_t>>& rev_adj,
                                            size_t* num_comps_out) const;
        
        /// returns the transitive reduction of a topologically sorted DAG's adjacency list
        vector<vector<size_t>> transitive_reduction(const vector<vector<size_t>>& adj) const;
        
        /// eliminate any path chunks that have the exact same colinearities as another but are much shorter
        vector<vector<size_t>> remove_dominated_chunks(const vector<vector<size_t>>& adj,
                                                       vector<path_chunk_t>& path_chunks,
                                                       vector<pair<step_handle_t, step_handle_t>>& ref_chunks,
                                                       vector<tuple<size_t, size_t, int32_t>>& connections) const;
        
        /// returns the nodes that are the center of a directed star through which all source-to-sink
        /// paths in their connected component flow. paired bool indicates whether the star faces
        /// forward (false) or backward (true).
        vector<pair<size_t, bool>> find_constriction_stars(const vector<vector<size_t>>& adj) const;
        
        /// make a sentinel meant to indicate an unmapped read
        static Alignment make_null_alignment(const Alignment& source);
        
        static multipath_alignment_t make_null_mp_alignment(const multipath_alignment_t& source);
        
        /// the graph we're surjecting onto
        const PathPositionHandleGraph* graph = nullptr;
    };
}

#endif
