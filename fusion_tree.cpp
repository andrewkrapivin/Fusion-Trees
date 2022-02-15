#include "fusion_tree.h"
#include "HelperFuncs.h"

#include <algorithm>
#include <iostream>

namespace FusionTree {

	inline uint64_t get_uint64_from_m256(__m256i vec, int pos) {
		__mmask8 pos_mask = _cvtu32_mask8(1 << pos);
		__m256i movinglongtofront = _mm256_maskz_compress_epi64(pos_mask, vec);
		__m128i lowerbytes = _mm256_extracti64x2_epi64(movinglongtofront, 0);
		uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
		return lowestlong;
	}
	inline uint64_t get_uint64_from_m512(__m512i vec, int pos) {
		__mmask8 pos_mask = _cvtu32_mask8(1 << pos);
		__m512i movinglongtofront = _mm512_maskz_compress_epi64(pos_mask, vec);
		__m128i lowerbytes = _mm512_extracti64x2_epi64(movinglongtofront, 0);
		uint64_t lowestlong = _mm_extract_epi64(lowerbytes, 0);
		return lowestlong;
	}

	int first_diff_bit_pos(__m512i x, __m512i y) {//there is def some confusion here with me about big endian/little endian so maybe need to change this a bit
		__m512i z = _mm512_xor_si512(x, y);
		__m512i w = _mm512_xor_si512(z, z);
		__mmask16 nonzero_pieces = _mm512_cmp_epi32_mask(z, w, _MM_CMPINT_NE);
		unsigned int nzr = _cvtmask16_u32(nonzero_pieces);
		if(nzr == 0) return -1; //-1 means the numbers are the same

		//we are looking for the "leading" zeros, but we want to reverse to get the bit position cause all of this stuff makes absolutely no sense
		unsigned int significant_one_index = 31 - _lzcnt_u32(nzr); // the naming of these functions is horrible! Because it counts trailizing zeros, but that's assuming big endian! But trailizing zeros should assume little endian, cause that's what the cpu sees! This causes even more confusion, as I'm thinking of implementing little endian I know also have to reverse my thinking here with the name
		__mmask16 significant_one_mask = _cvtu32_mask16(1 << significant_one_index);
		w = _mm512_maskz_compress_epi32(significant_one_mask, z); //must be a better way to do this
		__m128i lowerbytes = _mm512_extracti64x2_epi64(w, 0);
		unsigned int diffint = _mm_extract_epi32(lowerbytes, 0);
		return 32*significant_one_index + (31 - _lzcnt_u32(diffint));
	}

	inline int get_bit_from_pos(__m512i key, int pos) {//check if pos out of bounds?
		uint64_t element_containing_bit = get_uint64_from_m512(key, pos/64);
		return (element_containing_bit & (1ull << (pos%64))) != 0;
	}

	inline int get_bit_from_pos(__m256i key, int pos) {//check if pos out of bounds?
		uint64_t element_containing_bit = get_uint64_from_m256(key, pos/64);
		return (element_containing_bit & (1ull << (pos%64))) != 0;
	}

	bool compare__m512i(__m512i a, __m512i b) { //remember for some reaon compare in sort is assumed to evaluate a < b
		int diffbitpos = first_diff_bit_pos(a, b);
		if(diffbitpos == -1) return false;
		return get_bit_from_pos(b, diffbitpos);
	}

	inline __m256i setbit_each_epi16_in_range(__m256i src, int epi16pos, int low, int high, int bit /* = 1*/) {
		__mmask16 setmask = _cvtu32_mask16((1 << (high+1)) - (1 << low));
		__m256i addvec = _mm256_maskz_set1_epi16(setmask, bit << epi16pos);
		return _mm256_or_si256(src, addvec);
	}

	//Right specifies where the value is inserted. Is it inserted to the right of the sketch at the desired position or to the left?
	inline __m256i insert_sketch(__m256i sketches, uint16_t sketch, int pos, bool right) {
		if(right)
			pos++;
		uint16_t expand_mask_bits = (-1) ^ (1 << pos);
		__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
		sketches = _mm256_maskz_expand_epi16(expand_mask, sketches);
		__m256i addmask = _mm256_maskz_set1_epi16(_knot_mask16(expand_mask), sketch);
		sketches = _mm256_or_si256(addmask, sketches);
		return sketches;
	}

	inline __m256i insert_partial_duplicate_sketch(__m256i sketches, int pos, bool right, int cuttoff_pos) {
		uint16_t expand_mask_bits = (-1) ^ (1 << pos);
		__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
		__m256i maskvec_exp = _mm256_maskz_expand_epi16(expand_mask, sketches);
		__m256i ans = _mm256_mask_blend_epi16(expand_mask_bits, sketches, maskvec_exp);

		expand_mask = _knot_mask16(expand_mask);
		if(right) expand_mask = _kshiftli_mask16(expand_mask, 1);
		__m256i cuttoffmask = _mm256_maskz_set1_epi16(expand_mask, (1 << cuttoff_pos) - 1);
		return _mm256_andnot_si256(cuttoffmask, ans);
	}

	inline __m128i insert_sorted_pos_to_key_positions(__m128i key_positions, uint8_t sorted_pos, uint8_t real_pos, bool right) {
		if(!right)
			sorted_pos++;
		uint16_t expand_mask_bits = (-1) ^ (1 << sorted_pos);
		__mmask16 expand_mask = _cvtu32_mask16(expand_mask_bits);
		key_positions = _mm_maskz_expand_epi8(expand_mask, key_positions);
		__m128i addpos = _mm_maskz_set1_epi8(_knot_mask16(expand_mask), real_pos);
		key_positions = _mm_or_si128(addpos, key_positions);
		return key_positions;
	}

	inline __m256i shift_bit_in_sketches(__m256i sketches, int pos) {
		uint16_t keep_mask = (1 << pos) - 1;
		__mmask16 ones = 0xffff;
		__m256i lowerbits_mask = _mm256_maskz_set1_epi16(ones, keep_mask);
		__m256i lowerbits = _mm256_and_si256(sketches, lowerbits_mask);
		__m256i higherbits = _mm256_andnot_si256(lowerbits_mask, sketches);
		constexpr __m128i count = {1, 0};
		higherbits = _mm256_maskz_sll_epi16(ones, higherbits, count);
		return _mm256_or_si256(lowerbits, higherbits);
	}

	inline uint16_t shift_bit_in_sketch(uint16_t sketch, int pos) {
		uint16_t lowerbits_mask = (1 << pos) - 1;
		return (sketch & lowerbits_mask) + ((sketch & (~lowerbits_mask)) << 1); //classic ! vs ~
	}

	
	bool MiniTree::node_full(){
		return meta.size == MAX_FUSION_SIZE;
	}

	//wow big endian vs little endian really really sucks, but I think this function works
	inline uint16_t MiniTree::extract_bits(__m512i key) {
		__m512i extractedbytes = _mm512_maskz_compress_epi8(byte_extract, key);
		__m128i lowerbytes = _mm512_extracti64x2_epi64(extractedbytes, 0);
		uint64_t ebyteslong[2];
		ebyteslong[0] = _mm_extract_epi64(lowerbytes, 0);
		ebyteslong[1] = _mm_extract_epi64(lowerbytes, 1); //better way to do this?
		return (_pext_u64(ebyteslong[1], bitextract[1]) << _mm_popcnt_u64(bitextract[0])) + _pext_u64(ebyteslong[0], bitextract[0]);
	}

	inline __m256i MiniTree::compare_mask(uint16_t sketch) {
		__m256i cmpmask = _mm256_set1_epi16(sketch);
		return cmpmask;
	}

	inline __m256i FastInsertMiniTree::compare_mask(uint16_t sketch) {
		__m256i cmpmask = _mm256_set1_epi16(sketch);
		return _mm256_and_si256(cmpmask, ignore_mask);
	}

	inline int MiniTree::search_position(uint16_t sketch, const bool geq /*= true*/) {
		__m256i cmpmask = compare_mask(sketch);
		__mmask16 geq_mask;
		if(geq)
			geq_mask = _mm256_cmp_epi16_mask(sketches, cmpmask, _MM_CMPINT_LE);
		else
			geq_mask = _mm256_cmp_epi16_mask(sketches, cmpmask, _MM_CMPINT_LT);
		uint16_t converted = _cvtmask16_u32(geq_mask);
		//we need to ignore comparisons with stuff where its beyond the size!!!!
		return _mm_popcnt_u32(converted & ((1 << meta.size) - 1));
	}

	//returns the position in the array of the largest element less than or equal to the sketch
	inline int MiniTree::search_pos_arr(uint16_t sketch, const bool geq /*= true*/) {
		int pos = search_position(sketch, geq);
		pos = std::max (pos-1, 0);
		return pos;
	}

	//complete this
	inline int MiniTree::search_pos_tree(uint16_t sketch) {
		return -1;
	}

	//returns the position in the array of the element which would be reached by going down the blind trie using the sketch
	inline int FastInsertMiniTree::search_pos_tree(uint16_t sketch) {
		int pos_arr = search_pos_arr(sketch);
		//basically we want to see whether the pos_arr or pos_arr+1 matches the sketch more closely, cause pos_arr and pos_arr+1 obviously difffer in one bit and we want to see which "path" or branch sketch takes at that differing bit
		//clearly, if we've reached the rightmost node of the tree, we are done
		if(pos_arr == meta.size-1) return pos_arr;
		__mmask16 pos_mask = _cvtu32_mask16((1 << pos_arr) * 3); //we want the position of this element and the next position of course for our comparison
		__m256i movinglongtofront = _mm256_maskz_compress_epi16(pos_mask, _mm256_and_si256(sketches, ignore_mask));
		__m128i lowerbytes = _mm256_extracti64x2_epi64(movinglongtofront, 0);
		uint32_t lowestint = _mm_extract_epi32(lowerbytes, 0);
		//now we want the element which more closely matches, so the one with the smaller first differing bit
		const uint32_t otherkeypos = 1 << 16;
		lowestint ^= ((uint32_t) sketch) * (otherkeypos + 1); //xor each part to get first differing bit with each compressed key. We then just need to see which part is smaller!
		return ((lowestint%otherkeypos) < (lowestint/otherkeypos)) ? pos_arr : (pos_arr+1);
	}

	//basically finds either the smallest or largest leaf of an "internal node" in the tree--or here really the largest or smallest key that matches the sketch up to cutoff_pos
	inline int MiniTree::search_partial_pos_tree(uint16_t sketch, int cutoff_pos, bool largest) {
		uint16_t partial_basemask = sketch & (~((1 << cutoff_pos) - 1)); //remember to use ~ not ! for bitwise lol
		partial_basemask = largest ? (partial_basemask + ((1 << cutoff_pos) - 1)) : partial_basemask;
		return search_pos_arr(partial_basemask);
	}

	inline int MiniTree::search_partial_pos_tree2(uint16_t sketch, int cutoff_pos, bool largest) {
		uint16_t partial_basemask = sketch & (~((1 << cutoff_pos) - 1)); //remember to use ~ not ! for bitwise lol
		partial_basemask = largest ? (partial_basemask + ((1 << cutoff_pos) - 1)) : partial_basemask;
		return search_position(partial_basemask, largest);
	}

	uint8_t MiniTree::get_real_pos_from_sorted_pos(int index_in_sorted) {
		return index_in_sorted;
	}

	uint8_t FastInsertMiniTree::get_real_pos_from_sorted_pos(int index_in_sorted) {
		__mmask16 pos_mask = _cvtu32_mask16((1 << index_in_sorted)); //we want the position of this element and the next position of course for our comparison
		__m128i extracting_position = _mm_maskz_compress_epi8(pos_mask, key_positions);
		uint8_t position = _mm_extract_epi8(extracting_position, 0);
		return position;
	}

	__m512i MiniTree::get_key_from_sorted_pos(int index_in_sorted) {
		return keys[index_in_sorted];
	}

	__m512i FastInsertMiniTree::get_key_from_sorted_pos(int index_in_sorted) {
		__mmask16 pos_mask = _cvtu32_mask16((1 << index_in_sorted)); //we want the position of this element and the next position of course for our comparison
		__m128i extracting_position = _mm_maskz_compress_epi8(pos_mask, key_positions);
		uint8_t position = _mm_extract_epi8(extracting_position, 0);
		return keys[position];
	}

	__m512i MiniTree::search_key(uint16_t sketch) {
		int pos = search_position(sketch);
		pos = std::max (pos-1, 0);
		return get_key_from_sorted_pos(pos);
	}

	inline int MiniTree::diff_bit_to_sketch_pos(unsigned int diffpos) {
		uint64_t byte_extract_ll = _cvtmask64_u64(byte_extract);
		unsigned int diffposbyte = diffpos/8;
		bool test_specific_bits = byte_extract_ll & (1ll << diffposbyte);
		int numbelow = _mm_popcnt_u64(byte_extract_ll & ((1ll << diffposbyte) - 1));
		int maskpos = 0;
		if(numbelow > 0) {
			uint64_t tmp = std::min(numbelow, 8) * 8;
			uint64_t extract_mask = tmp == 64 ? (- 1ll) : ((1ll << tmp) - 1ll);
			maskpos += _mm_popcnt_u64(bitextract[0] & extract_mask);
		}
		if(numbelow > 8) {
			uint64_t tmp = std::min(numbelow-8, 8) * 8;
			maskpos += _mm_popcnt_u64(bitextract[1] & ((1ll << tmp) - 1ll));
		}
		if(test_specific_bits) {
			uint8_t tmp = *((uint8_t*)bitextract + numbelow);
			maskpos += _mm_popcnt_u64(tmp & ((1 << ((diffpos%8) + 1)) - 1));
			if(tmp & (1 << (diffpos%8)))
				maskpos *= -1;
		}
		return maskpos;
	}

	inline void MiniTree::add_position_to_extraction_mask(int pos_in_key) {
		//update the byte_extract sketch
		int byte_in_key = pos_in_key/8;
		__mmask64 addbit_to_byte_extract_mask = _cvtu64_mask64(1ull << byte_in_key);
		__mmask64 new_byte_extract = _kor_mask64(byte_extract, addbit_to_byte_extract_mask);
		
		uint64_t byte_extract_ll = _cvtmask64_u64(byte_extract);
		int numbelow = _mm_popcnt_u64(byte_extract_ll & ((1ll << byte_in_key) - 1));
		//if we changed byte extract, then we need to shift the bit extract over by one byte, which we can do with avx. Otherwise we just need to add the corresponding bit to bit extract
		if(!_kortestz_mask64_u8(_kxor_mask64(new_byte_extract, byte_extract), 0)) { //kortest gives one if the or is all zeros, but we want the two numbers to be different and thus not all zeros
			__m128i bitextract_to_shift = _mm_lddqu_si128((const __m128i*)&bitextract[0]);
			__mmask16 expand_mmask = _cvtu32_mask16 (~(1 << numbelow));
			_mm_storeu_si128((__m128i*)&bitextract[0], _mm_maskz_expand_epi8(expand_mmask, bitextract_to_shift));
		}
		bitextract[numbelow/8] |= (1ull << (((numbelow%8) * 8) + pos_in_key%8));
		byte_extract = new_byte_extract;
	}
	
	//Finish this, or just say inserting into regular minitree illegal
	int MiniTree::insert(__m512i key) {
		return -1;
	}

	int FastInsertMiniTree::insert(__m512i key) {
		if(meta.size == 0) { //Should we assume that key_positions[0] is zero?
			keys[0] = key;
			meta.size++;
			return 1;
		}

		if(meta.size == MAX_FUSION_SIZE)
			return -1;
			
		uint16_t first_basemask = extract_bits(key);
		
		int first_guess_pos = search_pos_tree(first_basemask);
		
		__m512i first_guess = get_key_from_sorted_pos(first_guess_pos);
		
		int diff_bit_pos = first_diff_bit_pos(first_guess, key);
		
		if (diff_bit_pos == -1) { //key is already in there
			return -2;
		}
		
		int mask_pos = diff_bit_to_sketch_pos(diff_bit_pos);
		int search_mask_pos = mask_pos;
		
		int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
		
		bool need_highest = !diff_bit_val; //if the diff_bit_val is one, then we already have the highest element that fits the sketch until this point!
		
		if (mask_pos >= 0) { //then we do not already have this bit in the "fusion tree." We thus need to shift our masks and stuff to include this bit
			sketches = shift_bit_in_sketches(sketches, mask_pos);
			ignore_mask = shift_bit_in_sketches(ignore_mask, mask_pos);
			//and we also shift the sketch itself! And set the corresponding bit to the corresponding value
			first_basemask = shift_bit_in_sketch(first_basemask, mask_pos);
			first_basemask |= diff_bit_val << mask_pos;
			
			add_position_to_extraction_mask(diff_bit_pos);
			
			//increment search_mask_pos cause we want to search all the elements that are in the same subtree, but the bit defining the subtree has now increased, since we added a so far nonsense bit into the masks
			search_mask_pos ++;
			//if its an unseen bit then we need to query first_guess_pos again to find where the actual insert position should be. Obviously this is pretty bad code, so fix this.
		}
		else {
			//probably not the best way to do this, but well simply just like if we have seen mask_pos, then well we dodn't want to affect stuff at the maskpos position cause that's actually where our last important bit is. Otherwise we did actually move everything one over
			mask_pos = -1 - mask_pos;
		}

		first_guess_pos = search_partial_pos_tree(first_basemask, mask_pos+1, !need_highest);
		
		int low_pos = need_highest ? first_guess_pos : search_partial_pos_tree(first_basemask, mask_pos+1, false);
		
		int high_pos = need_highest ? search_partial_pos_tree(first_basemask, mask_pos+1, true) : first_guess_pos;
		
		//we need to set ignore sketch to not ignore positions here
		ignore_mask = setbit_each_epi16_in_range(ignore_mask, mask_pos, low_pos, high_pos);
		
		//we need to set ignore sketch to not ignore positions here
		sketches = setbit_each_epi16_in_range(sketches, mask_pos, low_pos, high_pos, 1-diff_bit_val);
		
		//we need to insert the treebits in but we need to be careful about whether basically we are shifting the element at the first_guess_pos left or right, and we want to shift it based on whether our node is bigger or smaller than the element! So if the diffbit is one, we want to shift it left (smaller), and otherwise we shift right (bigger), which means 
		sketches = insert_sketch(sketches, first_basemask, first_guess_pos, diff_bit_val);
		
		//here I am inserting just don't ignore any bits, but I am not 100% sure that is correct. Is easier to do this than copying the guy we're next to, but that might be necessary. Def check this
		//node->ignore_mask = insert_sketch(node->ignore_mask, -1, first_guess_pos, diff_bit_val); //lol fixing insert_sketch took way longer than it should have
		//Pretty sure I came up with a situation where the above is wrong, but didn't work it out. Just being safe then:
		//actually we want to duplicate it only up to the bit which matters, so the mask_pos. Here I am really not sure its important either, but well let's be safe
		ignore_mask = insert_partial_duplicate_sketch(ignore_mask, first_guess_pos, diff_bit_val, mask_pos);

		//cause the sketch we inserted for our new element might have extra bits set that it should not have
		sketches = _mm256_and_si256(sketches, ignore_mask);
		
		key_positions = insert_sorted_pos_to_key_positions(key_positions, first_guess_pos, meta.size, !diff_bit_val); //idk where to move it left or right for now but just say it moves to the left
		
		keys[meta.size] = key;
		
		meta.size++;
		
		return 0;
	}

	int MiniTree::query_branch(__m512i key) {
		uint16_t first_basemask = extract_bits(key);
		
		int first_guess_pos = search_pos_tree(first_basemask);
		
		__m512i first_guess = get_key_from_sorted_pos(first_guess_pos);
		
		int diff_bit_pos = first_diff_bit_pos(first_guess, key);
		
		if (diff_bit_pos == -1) { //key is already in there
			return ~first_guess_pos; //idk if its negative then let's say you've found the exact key
		}
		
		int mask_pos = diff_bit_to_sketch_pos(diff_bit_pos);
		int diff_bit_val = get_bit_from_pos(key, diff_bit_pos);
		int second_guess_pos = search_partial_pos_tree2(first_basemask, abs(mask_pos), diff_bit_val);
		return second_guess_pos;
	}

	void MiniTree::print_keys_sig_bits() {
		for(int i=0; i<meta.size; i++) {
			print_binary_uint64_big_endian(get_key_from_sorted_pos(i)[7], true, 64, 8);
		}
	}
	
	void MiniTree::print_node_info() {
	}

	void FastInsertMiniTree::print_node_info() {
		std::cout << "Extraction mask:" << std::endl;
		print_binary_uint64(byte_extract, true);
		print_binary_uint64(bitextract[0], false, 8);
		std::cout << " ";
		print_binary_uint64(bitextract[1], true, 8);
		std::cout << "Tree and ignore bits:" << std::endl;
		print_vec(sketches, true, 16);
		print_vec(ignore_mask, true, 16);
		std::cout << "Sorted to real positions:" << std::endl;
		print_vec(key_positions, true, 8);
	}
}