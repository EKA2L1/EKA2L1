#include <common/flate.h>
#include <common/log.h>

namespace eka2l1 {
    namespace flate {

        namespace huffman {
            using huff = uint16_t;
            using v2u32p = std::pair<uint32_t, uint32_t>;

            const huff leaf = 0x8000;

            struct node {
                uint32_t count;
                huff left;
                huff right;
            };

            const uint32_t huffman_enc[]=
                {
                0x10000000,
                0x1c000000,
                0x12000000,
                0x1d000000,
                0x26000000,
                0x26800000,
                0x2f000000,
                0x37400000,
                0x37600000,
                0x37800000,
                0x3fa00000,
                0x3fb00000,
                0x3fc00000,
                0x3fd00000,
                0x47e00000,
                0x47e80000,
                0x47f00000,
                0x4ff80000,
                0x57fc0000,
                0x5ffe0000,
                0x67ff0000,
                0x77ff8000,
                0x7fffa000,
                0x7fffb000,
                0x7fffc000,
                0x7fffd000,
                0x7fffe000,
                0x87fff000,
                0x87fff800
             };

            // Give a tree and a root index, emit the length of from that node to the current node
            // to the length array
            void huffman_len(uint32_t* lengths, const node* nodes, int cnode, int len) {
                if (++len > HUFFMAN_MAX_CODELENGTH) {
                    LOG_ERROR("Buffer overflowed for huffman");
                    return;
                }

                const node tree_node = nodes[cnode];

                huff left_node = tree_node.left;

                if (left_node & leaf)
                    lengths[left_node & ~leaf] = len;
                else
                     huffman_len(lengths,nodes,left_node,len);

                huff right_node = tree_node.right;

                if (right_node & leaf)
                    lengths[right_node & ~leaf]= len;
                else
                    huffman_len(lengths,nodes,right_node,len);
            }

            void insert_in_order(node* nodes, uint32_t size, v2u32p val) {
                uint32_t l = 0;
                uint32_t r = size;

                while (l < r) {
                    uint32_t m = (l+r) >> 1;

                    if (nodes[m].count < val.first)
                        r = m;
                    else
                        l = m + 1;
                }

                for (uint32_t i = size - 1; i >= l; i--)
                    nodes[i+1] = nodes[i];

                nodes[l].count = val.first;
                nodes[l].right = val.second;
            }

            bool huffman(const int* freq, uint32_t num_codes, int* huffman) {
                if (num_codes > HUFFMAN_MAX_CODELENGTH) {
                    LOG_ERROR("Too much codes for huffman decoding!");
                    return false;
                }

                // Sort the values into decreasing order of frequency using insert sort
                std::vector<node> nodes(num_codes);
                uint32_t crr = 0;

                for (uint32_t i = 0; i < num_codes; ++i) {
                    uint32_t c = freq[i];

                    if (c != 0)
                        insert_in_order(nodes.data(), crr++,
                                        v2u32p(c, i | leaf));
                }

                std::fill(huffman, huffman + num_codes, 0);

                if (crr == 0) {
                }
                else if (crr == 1) {
                    // Special case for a single value (always encode as "0")
                   huffman[nodes[0].right &~ leaf]=1;
                }
                else
                {
                    // Huffman algorithm: pair off least frequent nodes and reorder
                    do {
                        --crr;
                        uint32_t c = nodes[crr].count + nodes[crr].count;
                        nodes[crr].left=nodes[crr-1].right;
                        // Re-order the leaves now to reflect new combined frequency 'c'
                        insert_in_order(nodes.data(), crr-1, v2u32p(c, crr));
                    } while (crr>1);

                    // Generate code lengths
                    huffman_len(reinterpret_cast<uint32_t*>(huffman),nodes.data(),1,0);
                 }

                if(!valid(huffman, num_codes)) {
                    LOG_ERROR("Huffman invalid code!");
                    return false;
                };

                return true;
            }

            bool valid(const int* huffman,int num_codes) {
                uint32_t remain = 1 << HUFFMAN_MAX_CODELENGTH;
                int total_len = 0;

                // Downing
                for (const int* p = huffman + num_codes; p > huffman;) {
                    int len = *--p;

                    if (len>0) {
                        total_len += len;

                        if (len > HUFFMAN_MAX_CODELENGTH)
                            return false;

                        uint32_t c = 1 << (HUFFMAN_MAX_CODELENGTH-len);

                        if (c > remain)
                            return false;

                        remain -= c;
                    }
                }

                return remain==0 || total_len <= 1;
            }

            void encoding(const int* huffman, uint32_t num_codes, int* encode_tab) {
                std::array<uint32_t, HUFFMAN_MAX_CODELENGTH> len_count;

                int i = 0;

                for (i = 0; i< num_codes; ++i) {
                    int len = huffman[i]-1;
                    if (len >= 0)
                        ++len_count[len];
                }

                std::array<uint32_t, HUFFMAN_MAX_CODELENGTH> next_code;
                uint32_t code = 0;

                for (i = 0; i < HUFFMAN_MAX_CODELENGTH; ++i) {
                    code <<= 1;
                    next_code[i] = code;
                    code += len_count[i];
                }

                for (i = 0; i < num_codes; ++i) {
                    int len = huffman[i];

                    if (len == 0)
                        encode_tab[i]=0;
                    else {
                        encode_tab[i] = (next_code[len-1] << (HUFFMAN_MAX_CODELENGTH - len)) | (len << HUFFMAN_MAX_CODELENGTH);
                        ++next_code[len-1];
                    }

                }
            }

            void encode_runlen(bit_output& output, int len) {
                if (len > 0)
                {
                    // Keep encode by spliting it
                    encode_runlen(output,(len-1)>>1);
                    output.huffman(huffman_enc[1 - (len & 1)]);
                }
            }

            void externalize(bit_output& output,const uint32_t* huff_man, uint32_t num_codes) {
                std::array<uint8_t, HUFFMAN_METACODE> list;
                int i = 0;

                for (i = 0; i < list.size(); ++i)
                    list[i] = i;
                int last = 0;

                int rl = 0;
                const uint32_t* p32 = huff_man;
                const uint32_t* e32 = p32 + num_codes;

                while (p32 < e32) {
                    int c = *p32++;

                    if (c == last)
                        ++rl;
                    else {
                        // encode run-length
                        encode_runlen(output, rl);
                        rl=0;

                        int j = 0;
                        for (j = 1; list[j] !=c; ++j)
                            ;

                        // store this code
                        output.huffman(huffman_enc[j+1]);

                        // adjust list for MTF algorithm
                        while (--j > 0)
                            list[j+1]=list[j];

                        list[1] = last;
                        last=c;
                    }
                }

                // Encode left over
                encode_runlen(output, rl);
            }

            const int huff_term=0x0001;
            const uint32_t branch1 = sizeof(uint32_t) << 16;

            // Do a binary search and insert subtree
            uint32_t* huffman_subtree(uint32_t* ptr, const uint32_t* val, uint32_t** lvl) {
                uint32_t* left = *lvl++;
                if (left > val) {
                    uint32_t* sub0 = huffman_subtree(ptr, val, lvl);  // 0-tree first
                    ptr = huffman_subtree(sub0, val - (ptr - sub0) - 1, lvl);  // 1-tree
                    int branch0 = (uint8_t*)sub0 - (uint8_t*)(ptr - 1);
                    *--ptr = branch1 | branch0;
                } else if (left == val) {
                    uint32_t term0 = *val--;    // 0-term
                    ptr = huffman_subtree(ptr, val, lvl);			// 1-tree
                    *--ptr = branch1 | (term0>>16);
                } else	{       // l<iNext
                    uint32_t term0 = *val--;						// 0-term
                    uint32_t term1 = *val--;
                    *--ptr = (term1 >> 16 <<16) |(term0 >> 16);
                }
                return ptr;
            }

            void decoding(const int* huffman, uint32_t num_codes, uint32_t* decode_tree,int sym_base) {
                if (!valid(huffman, num_codes)) {
                    LOG_ERROR("Decoding failed: Huffman codes invalid!");
                    return;
                }

                std::array<uint32_t, HUFFMAN_MAX_CODELENGTH> counts;
                uint32_t codes = 0;

                int i = 0;

                for (i = 0; i < num_codes; ++i) {
                     int32_t len = huffman[i];
                     decode_tree[i] = len;

                     if (--len >= 0) {
                        ++counts[len];
                        ++codes;
                     }
                }

                std::array<uint32_t*, HUFFMAN_MAX_CODELENGTH> lvl;
                uint32_t* lit = decode_tree + codes;

                for (i = 0; i < HUFFMAN_MAX_CODELENGTH; ++i) {
                    lvl[i] = lit;
                    lit -= counts[i];
                }

                sym_base = (sym_base << 17) + (huff_term << 16);

                for (i = 0; i < HUFFMAN_MAX_CODELENGTH; ++i) {
                    uint32_t len= (uint8_t)decode_tree[i];

                    if (len)
                        *--lvl[len-1] |= (i << 17) + sym_base;
                }

                if (codes == 1) {
                    uint8_t term = decode_tree[0] >> 16;
                    decode_tree[0] = term |(term<<16);
                }
                else if (codes>1) {
                    huffman_subtree(decode_tree + codes - 1, decode_tree + codes - 1,&lvl[0]);
                }
            }

            void externalize(bit_input& input, uint32_t* huffman,int num_codes) {
                std::array<uint8_t, HUFFMAN_METACODE> list;

                for (uint8_t i = 0; i < list.size(); ++i)
                    list[i]= uint8_t(i);

                char last=0;
                uint32_t* p = huffman;

                const uint32_t* end = huffman + num_codes;

                char rl = 0;

                while (p + rl < end) {
                    uint32_t c = input.huffman(huffman_enc);

                    if (c < 2) {
                        // Update run length
                        rl += rl + c + 1;
                    }
                    else {
                        while (rl > 0) {
                            if (p > end) {
                                LOG_ERROR("Externalize error: Invalid inflate data!");
                                return;
                            }

                            *p++ = last;
                            --rl;
                        }

                        --c;
                        list[0] = uint8_t(last);
                        last = list[c];

                        for (uint8_t i = c-1; i >= 0; i--)
                            list[i+1] = list[i];

                        if (p > end) {
                            LOG_ERROR("Externalize error: Invalid inflate data!");
                            return;
                        }
                        *p++ = last;
                    }
                }

                // Run length still left
                while (rl > 0)
                    {
                    if (p > end) {
                        LOG_ERROR("Externalize error: Invalid inflate data!");
                        return;
                    }

                    *p++ = last;
                    --rl;
                }
            }
        }

        void bit_output::do_write(int bits, uint32_t size) {
            if (size>25) {
                // Cannot process > 25 bits in a single pass
                // Do the top 8 bits first
                write(bits & 0xff000000u, 8);
                bits <<= 8;
                size -= 8;
            }

            int tbits = bits;
            uint32_t tcode = code |(bits >> (tbits+8));
            tbits += size;

            if (tbits>=0) {
               uint8_t* ptr = start;

               do {
                 if (ptr == end) {
                     // Are we here already
                     start = ptr;
                     LOG_ERROR("Buffer overflow when write output bits!");
                     ptr = start;
                 }

                 *ptr++ = (uint8_t)(code>>24);
                 tcode<<=8;
                 tbits-=8;
               } while (tbits>=0);

               start = ptr;
            }

            code = tcode;
            bits = tbits;
        }

        bit_output::bit_output() {}

        bit_output::bit_output(uint8_t* buf, size_t size)
            : start(buf), end(buf+size), bits(-8) {}

        void bit_output::set(uint8_t* buf, size_t size) {
            start = buf;
            end = buf+size;
        }

        uint8_t* bit_output::data() const {
            return start;
        }

        uint32_t bit_output::buffered_bits() const {
            return bits + 8;
        }

        void bit_output::write(uint32_t val, uint32_t len) {
            if (len)
                do_write(val <<= 32 - len, len);
        }

        void bit_output::huffman(uint32_t huff_code) {
            do_write(huff_code << (32 - HUFFMAN_MAX_CODELENGTH),
                     huff_code >> HUFFMAN_MAX_CODELENGTH);
        }

        void bit_output::pad(uint32_t pad_size) {
            if (bits > -8)
                do_write(pad_size ? 0xffffffffu : 0, -bits);
        }
    }
}
