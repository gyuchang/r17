// Copyright 2012 Matthew Nourse and n plus 1 computing pty limited unless otherwise noted.
// Please see LICENSE file for details.
#ifndef NP1_REL_JOIN_ANTI_HPP
#define NP1_REL_JOIN_ANTI_HPP


#include "np1/io/mandatory_record_input_stream.hpp"
#include "np1/io/gzfile.hpp"
#include "np1/consistent_hash_table.hpp"
#include "np1/rel/detail/join_helper.hpp"


namespace np1 {
namespace rel {


class join_anti {
public:
  template <typename Input_Stream, typename Output_Stream>
  void operator()(Input_Stream &input, Output_Stream &output,
                  const rstd::vector<rel::rlang::token> &tokens) {  
    /* Get the arguments. */
    rstd::string file_name2(rel::rlang::compiler::eval_to_string_only(tokens));
          
    /* Get the list of headers from the first file. */
    record file1_headers(input.parse_headings());                
  
    /* Get the list of headers from the second file. */
    io::gzfile file2;
    if (!file2.open_ro(file_name2.c_str())) {
      NP1_ASSERT(false, "Unable to open input file " + file_name2);
    }

    io::mandatory_record_input_stream<io::gzfile, record, record_ref> file2_stream(file2);
    record file2_headers(file2_stream.parse_headings());              
                
    /* Figure out which headings are common and not common. */
    rstd::vector<rstd::string> common_heading_names;  
    rstd::vector<size_t> file2_non_common_field_numbers;  

    detail::join_helper::find_common_and_non_common_headings(
      file1_headers, file2_headers, common_heading_names, file2_non_common_field_numbers);

    // Now read file2 into memory.
    detail::compare_specs compare_specs2(file2_headers, common_heading_names);
    detail::record_multihashmap<detail::join_helper::empty_type> map2(compare_specs2);
    file2_stream.parse_records(detail::join_helper::insert_record_callback(map2));
    
    // Close file2 'cause it might be using a big buffer and we can use all the RAM we can lay our mitts on.
    file2.close();
        
    // Set up the compare specs for file1.
    detail::compare_specs compare_specs1(file1_headers, common_heading_names);
    
    // Check that the join participants are all data types we support.
    detail::join_helper::validate_compare_specs(compare_specs1);
    detail::join_helper::validate_compare_specs(compare_specs2);

    // Antijoin- write out all records in file1 that have no match
    // in file2.  First, write out the headers.
    file1_headers.write(output);
    
    // Now read in file1 and antimerge :) as we go.
    input.parse_records(
      anti_merge_record_callback<Output_Stream>(output, map2, compare_specs1));
  }

private:  
  // The record callback for when we're antijoining the file1 stream with 
  // file2 (in memory).
  template <typename Output>
  struct anti_merge_record_callback {
    anti_merge_record_callback(
        Output &output, detail::record_multihashmap<detail::join_helper::empty_type> &map2,
        const detail::compare_specs &specs1)
    : m_output(output)
    , m_map2(map2)
    , m_specs1(specs1) {}
    
    // The record_ref we get here is from file1.
    bool operator()(const record_ref &ref1) const {
      if (!m_map2.find(ref1, m_specs1)) {
        ref1.write(m_output);
      }
      
      return true;
    }
    
    Output &m_output;
    detail::record_multihashmap<detail::join_helper::empty_type> &m_map2;
    detail::compare_specs m_specs1;
    rstd::vector<size_t> m_file2_non_common_field_numbers;
  };
};

} // namespaces
}


#endif
