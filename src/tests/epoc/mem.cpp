/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <config/config.h>
#include <mem/allocator/std_page_allocator.h>
#include <mem/chunk.h>
#include <mem/control.h>
#include <mem/process.h>

using namespace eka2l1;

namespace {
    struct mem_model_fixture {
        config::state conf;
        mem::basic_page_table_allocator alloc;
        mem::control_impl control;
        mem::mem_model_process_impl process;

        explicit mem_model_fixture(const mem::mem_model_type model) {
            control = mem::make_new_control(nullptr, &alloc, &conf, 12, false, model);
            process = mem::make_new_mem_model_process(control.get(), model);
        }
    };

    // No region flag at all, so the model finds no address-space section to
    // allocate from and refuses the chunk. This is the cheapest deterministic
    // failure both models share; the expensive one (exhausting a section) takes
    // the same path out of do_create().
    mem::mem_model_chunk_creation_info unsatisfiable_chunk() {
        mem::mem_model_chunk_creation_info info;
        info.size = 0x1000;
        info.flags = mem::MEM_MODEL_CHUNK_TYPE_NORMAL;
        info.perm = prot_read_write;

        return info;
    }
}

TEST_CASE("chunk_creation_failure_leaves_the_out_pointer_alone", "mem") {
    // A refused chunk must not publish a half-built struct. kernel::chunk only
    // logs when the model says no, so whatever is left in this pointer is what
    // the guest gets a handle to -- and it dereferences it on the next call.
    for (const auto model : { mem::mem_model_type::multiple, mem::mem_model_type::flexible }) {
        mem_model_fixture fixture(model);

        mem::mem_model_chunk *sentinel = reinterpret_cast<mem::mem_model_chunk *>(0x1);
        mem::mem_model_chunk *chunk = sentinel;

        REQUIRE(fixture.process->create_chunk(chunk, unsatisfiable_chunk()) != mem::MEM_MODEL_CHUNK_ERR_OK);
        REQUIRE(chunk == sentinel);
    }
}

TEST_CASE("chunk_creation_failure_does_not_leak_a_slot", "mem") {
    // The multiple model reserves a chunk slot before do_create() runs, and a
    // process only gets MAX_CHUNK_ALLOW_PER_PROCESS of them. Keeping the slot on
    // failure turns a retry loop into the very exhaustion that made it fail:
    // the code-chunk retry in the loader leaks one slot per attempt.
    mem_model_fixture fixture(mem::mem_model_type::multiple);

    for (int i = 0; i < 2048; i++) {
        mem::mem_model_chunk *chunk = nullptr;
        REQUIRE(fixture.process->create_chunk(chunk, unsatisfiable_chunk()) != mem::MEM_MODEL_CHUNK_ERR_OK);
    }

    // More failures than the process has slots, so a leak is certain to show
    // here rather than depending on how many the caller happened to burn.
    mem::mem_model_chunk_creation_info good = unsatisfiable_chunk();
    good.flags |= mem::MEM_MODEL_CHUNK_REGION_USER_LOCAL;

    mem::mem_model_chunk *chunk = nullptr;
    REQUIRE(fixture.process->create_chunk(chunk, good) == mem::MEM_MODEL_CHUNK_ERR_OK);
    REQUIRE(chunk != nullptr);

    fixture.process->delete_chunk(chunk);
}

TEST_CASE("a_global_chunk_outlives_the_process_that_created_it", "mem") {
    // Attaching to a chunk is not owning it: kernel::chunk::open_to() maps a global chunk
    // (FbsSharedChunk, WsGlobalMemChunk, the skin chunk) into every process that opens a
    // handle, and Symbian keeps such a chunk alive for as long as a handle exists. A process
    // exiting must therefore not free a chunk struct somebody else still has mapped -- the
    // survivor's attach info points straight at it.
    //
    // Without the fix this is a use-after-free rather than a wrong value, so it reports as a
    // pass on an ordinary build and is caught under a sanitiser. It is written as a test
    // anyway: it pins the ownership rule, and it is exactly the shape of regression an
    // ASan job would exist to catch.
    config::state conf;
    mem::basic_page_table_allocator alloc;
    mem::control_impl control = mem::make_new_control(nullptr, &alloc, &conf, 12, false,
        mem::mem_model_type::flexible);

    auto creator = mem::make_new_mem_model_process(control.get(), mem::mem_model_type::flexible);
    auto opener = mem::make_new_mem_model_process(control.get(), mem::mem_model_type::flexible);

    mem::mem_model_chunk_creation_info info = unsatisfiable_chunk();
    info.flags |= mem::MEM_MODEL_CHUNK_REGION_USER_GLOBAL;

    mem::mem_model_chunk *chunk = nullptr;
    REQUIRE(creator->create_chunk(chunk, info) == mem::MEM_MODEL_CHUNK_ERR_OK);
    REQUIRE(chunk != nullptr);

    // A second process opens a handle to it and gets its own mapping.
    REQUIRE(opener->attach_chunk(chunk));

    // The creator exits first.
    creator.reset();

    // The chunk is still mapped by the opener, so it must still be there.
    REQUIRE(chunk->max() >= 0x1000);
    REQUIRE(opener->detach_chunk(chunk));
}
