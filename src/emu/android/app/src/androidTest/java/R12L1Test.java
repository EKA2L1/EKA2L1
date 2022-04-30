import androidx.test.filters.LargeTest;
import org.junit.Before;
import org.junit.Test;
import org.junit.Assert;

import com.github.eka2l1.emu.Emulator;

@LargeTest
public class R12L1Test {
    static {
        System.loadLibrary("native-lib");
    }

    @Test
    public void imbRangeSimpleInvalidationNoLink() {
        Assert.assertTrue("Cache invalidation simple test failed",
                Emulator.runTest("simple_invalidation_no_link"));
    }

    @Test
    public void regCacheStressFullRegsUsed() {
        Assert.assertTrue("Use full registers test failed",
                Emulator.runTest("use_full_regs"));
    }

    @Test
    public void loadSingleDoubleVFPChain() {
        Assert.assertTrue("Load single/double VFP chain test failed!",
                Emulator.runTest("load_chain_single_double"));
    }

    @Test
    public void loadNearPageCrossingTestNoTlbMiss() {
        Assert.assertTrue("Load near page crossing with no TLB miss test failed",
            Emulator.runTest("load_near_page_crossing_with_no_tlb_miss"));
    }

    @Test
    public void storeNearPageCrossingTestNoTlbMiss() {
        Assert.assertTrue("Store near page crossing with no TLB miss test failed!",
            Emulator.runTest("store_near_page_crossing_with_no_tlb_miss"));
    }

    @Test
    public void storeDaNearPageCrossingTestNoTlbMiss() {
        Assert.assertTrue("Store decrease after near page crossing with no TLB miss test failed!",
                Emulator.runTest("store_da_near_page_crossing_with_no_tlb_miss"));
    }
}