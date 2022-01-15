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
}