#ifndef FUSION_TREE_H_INCLUDED
#define FUSION_TREE_H_INCLUDED

#include <immintrin.h>
#include <cstdint>

#define MAX_FUSION_SIZE 16

namespace FusionTree {

	inline uint64_t get_uint64_from_m256(__m256i vec, int pos);
	inline uint64_t get_uint64_from_m512(__m512i vec, int pos);

	int first_diff_bit_pos(__m512i x, __m512i y); // seems works

	inline int get_bit_from_pos(__m256i key, int pos);
	inline int get_bit_from_pos(__m512i key, int pos);

	bool compare__m512i(__m512i a, __m512i b);

	inline __m256i setbit_each_epi16_in_range(__m256i src, int epi16pos, int low, int high, int bit = 1);

	inline __m256i insert_sketch(__m256i sketches, uint16_t sketch, int pos, bool right); //insert 16 bit sketch into some position (specifying which sketch)
	inline __m256i insert_partial_duplicate_sketch(__m256i sketches, int pos, bool right, int cuttoff_pos);

	inline __m128i insert_sorted_pos_to_key_positions(__m128i key_positions, uint8_t sorted_pos, uint8_t real_pos, bool right);

	inline __m256i shift_bit_in_sketches(__m256i sketches, int pos); //moves the bits to the left of pos one to the left to make room for a new bit there. pos is specifying bit in sketch.

	inline uint16_t shift_bit_in_sketch(uint16_t sketch, int pos);

	typedef struct {
	 	uint8_t size;
	 	uint8_t remaining[7];
	} Metadata;

	class MiniTree {
		protected:
			__m256i sketches;
			__mmask64 byte_extract;
			uint64_t bitextract[2];

		public:
			Metadata meta;
			__m512i keys[MAX_FUSION_SIZE];

			MiniTree(): sketches{0}, byte_extract(0), bitextract{0}, meta{0}, keys{0} {};
			
			bool node_full();

			inline uint16_t extract_bits(__m512i key); // seems works

			inline __m256i compare_mask(uint16_t sketch);

			inline int search_position(uint16_t sketch, const bool geq=true);
			inline int search_pos_arr( uint16_t sketch, const bool geq=true);
			inline int search_pos_tree(uint16_t sketch);
			inline int search_partial_pos_tree(uint16_t sketch, int cutoff_pos, bool largest);
			inline int search_partial_pos_tree2(uint16_t sketch, int cutoff_pos, bool largest);

			uint8_t get_real_pos_from_sorted_pos(int index_in_sorted);
			__m512i get_key_from_sorted_pos(int index_in_sorted);
			__m512i search_key(uint16_t sketch);

			inline int diff_bit_to_sketch_pos(unsigned int diffpos);

			inline void add_position_to_extraction_mask(int pos_in_key); // seems works

			int insert(__m512i key);

			int query_branch(__m512i key); //for now returns the bitwise complement if key is found

			void print_keys_sig_bits();
			void print_node_info();
	};

	class FastInsertMiniTree : public MiniTree {
		__m256i ignore_mask;
	 	__m128i key_positions;

		 public:
		 	FastInsertMiniTree(): ignore_mask{0}, key_positions{0} {};
		 	inline __m256i compare_mask(uint16_t sketch);
			inline int search_pos_tree(uint16_t sketch);
		 	int insert(__m512i key);
			uint8_t get_real_pos_from_sorted_pos(int index_in_sorted);
			__m512i get_key_from_sorted_pos(int index_in_sorted);
			void print_node_info();

	};

	// typedef struct {
	// 	uint8_t size;
	// 	uint8_t remaining[7];
	// } fusion_metadata;

	// typedef struct {
	// 	__m256i treebits;
	// 	//we want to extract 16 bits from 512 bit key, so we have one sketch that extracts the bytes that hold the keys, and then we do 2 cycles of pext to get the bits we need
	// 	__mmask64 byte_extract;
	// 	uint64_t bitextract[2]; //we have to keep these in order, and for maintaining the least endian order that cpu uses (we sometimes treat this as a single number to do ops on, we actually say that the bitextract[0] deals with the *higher* bits. Should this actually just be stored as an _mm128i? We could even popcnt in parallel, then just need to add the two pieces.
	// 	//idk what to do with this but this struct remains 64 bits from a 512 bit struct so this could store stuff about children which could speed up balancing
	// 	fusion_metadata meta;
	// } fusion_tree;

	// extern const fusion_tree Empty_Fusion_Tree;

	// /*typedef struct fast_insert_tree {
	// 	fustion_tree tree;
	// 	_m256i ignore_mask;
	// }*/

	// //for now implementing this fast fusion tree as default, but maybe segment fusion tree into two pieces, cause they have different ways of searching, inserting
	// typedef struct {
	// 	fusion_tree tree;
	// 	__m256i ignore_mask; //idk what exactly to do with this
	// 	__m128i key_positions; //convert key position assuming sorted array to the real position in the keys array
	// 	//also maybe add a vector for the free_positions to support deletions easily
	// 	__m512i keys[MAX_FUSION_SIZE];
	// } fusion_node;

	// extern const fusion_node Empty_Fusion_Node;

	// /*typedef struct fast_fusion_node {
		
	// }*/

}

#endif 
