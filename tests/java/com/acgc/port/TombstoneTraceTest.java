package com.acgc.port;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public final class TombstoneTraceTest {
    private static byte[] join(byte[]... parts) {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        for (byte[] part : parts) output.write(part, 0, part.length);
        return output.toByteArray();
    }
    private static byte[] varint(long value) {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        do { int b = (int)(value & 127); value >>>= 7; output.write(b | (value == 0 ? 0 : 128)); } while (value != 0);
        return output.toByteArray();
    }
    private static byte[] number(int field, long value) { return join(varint(field * 8L), varint(value)); }
    private static byte[] bytes(int field, byte[] value) { return join(varint(field * 8L + 2), varint(value.length), value); }
    private static byte[] string(int field, String value) { return bytes(field, value.getBytes(StandardCharsets.UTF_8)); }
    private static void require(boolean condition) { if (!condition) throw new AssertionError(); }
    private static void reject(byte[] input) throws IOException {
        try { TombstoneTrace.format(input); } catch (IOException expected) { return; }
        throw new AssertionError("Accepted corrupt tombstone");
    }
    public static void main(String[] args) throws Exception {
        byte[] signal = join(string(2, "SIGSEGV"), string(4, "SEGV_MAPERR"), number(8, 1), number(9, 0xabcdef1234L));
        byte[] frame = join(number(1, 0x12345678L), string(4, "production_function"), number(5, 0x128),
                string(6, "/data/app/lib/arm64/libmain.so"), string(8, "aabbccdd"));
        byte[] thread = join(string(2, "SDLThread"), bytes(4, frame));
        byte[] entry = join(bytes(2, thread), number(1, 123)); // map value before key
        byte[] trace = join(bytes(16, entry), number(6, 123), number(1, 1), bytes(10, signal),
                string(14, "test abort"), bytes(70, new byte[] {0, 1, 2}));
        String report = TombstoneTrace.format(trace);
        for (String required : new String[] {"ARM64", "tid=123", "SIGSEGV", "SEGV_MAPERR", "abcdef1234", "SDLThread",
                "12345678", "production_function+0x128", "libmain.so", "BuildId: aabbccdd", "test abort"})
            require(report.contains(required));
        require(TombstoneTrace.format(join(number(6, 123), bytes(16, entry))).contains("ARM32"));
        require(TombstoneTrace.format(number(1, 1)).contains("no incluyó"));
        reject(new byte[0]);
        reject(new byte[] {0});
        reject(new byte[] {10, 127, 1});
        reject(Arrays.copyOf(trace, trace.length - 1));
        reject(new byte[] {8, (byte)128, (byte)128, (byte)128, (byte)128, (byte)128,
                (byte)128, (byte)128, (byte)128, (byte)128, 2});
        reject(new byte[4 * 1024 * 1024 + 1]);
        System.out.println("PASS tombstone parser: ARM64 addresses, ARM32, unordered fields, unknown fields, corrupt/truncated input");
    }
}
