package com.acgc.port;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

/** Bounded reader for the public debuggerd tombstone.proto crash/backtrace fields.
 * Unknown fields (including memory dumps) are skipped, never interpreted as text.
 * Schema: android.googlesource.com/platform/system/core/+/master/debuggerd/proto/tombstone.proto
 */
final class TombstoneTrace {
    private TombstoneTrace() {}

    static String format(byte[] bytes) throws IOException {
        if (bytes.length == 0 || bytes.length > 4 * 1024 * 1024) throw new IOException("Tamaño de informe inválido");
        Reader root = new Reader(bytes, 0, bytes.length);
        long tid = 0, arch = 0;
        String signal = "", abort = "";
        Map<Long, String> threads = new LinkedHashMap<>();
        while (root.next()) {
            if (root.field == 1 && root.wire == 0) arch = root.number;
            else if (root.field == 6 && root.wire == 0) tid = root.number;
            else if (root.field == 10 && root.wire == 2) signal = signal(root.child());
            else if (root.field == 14 && root.wire == 2) abort = root.string();
            else if (root.field == 16 && root.wire == 2 && threads.size() < 128) {
                Reader entry = root.child();
                long key = 0;
                String trace = "";
                while (entry.next()) {
                    if (entry.field == 1 && entry.wire == 0) key = entry.number;
                    else if (entry.field == 2 && entry.wire == 2) trace = thread(entry.child());
                }
                threads.put(key, trace);
            }
        }
        String abi = arch == 0 ? "ARM32" : arch == 1 ? "ARM64" : "arch=" + arch;
        return abi + ", tid=" + tid + "\n" + signal + "\n"
                + (abort.isEmpty() ? "" : "Abort: " + abort + "\n")
                + threads.getOrDefault(tid, "Android no incluyó la pila del hilo que falló.\n");
    }

    private static String signal(Reader r) throws IOException {
        String name = "", code = "";
        long address = 0;
        boolean hasAddress = false;
        while (r.next()) {
            if (r.field == 2 && r.wire == 2) name = r.string();
            else if (r.field == 4 && r.wire == 2) code = r.string();
            else if (r.field == 8 && r.wire == 0) hasAddress = r.number != 0;
            else if (r.field == 9 && r.wire == 0) address = r.number;
        }
        return name + " " + code + (hasAddress ? " fault=0x" + Long.toHexString(address) : "");
    }

    private static String thread(Reader r) throws IOException {
        String name = "";
        StringBuilder frames = new StringBuilder();
        int count = 0;
        while (r.next()) {
            if (r.field == 2 && r.wire == 2) name = r.string();
            else if (r.field == 4 && r.wire == 2 && count < 64) {
                frames.append('#').append(count++).append(' ').append(frame(r.child())).append('\n');
            }
        }
        return "Thread: " + name + "\n" + frames;
    }

    private static String frame(Reader r) throws IOException {
        String file = "", function = "", build = "";
        long pc = 0, offset = 0;
        while (r.next()) {
            if (r.field == 1 && r.wire == 0) pc = r.number;
            else if (r.field == 4 && r.wire == 2) function = r.string();
            else if (r.field == 5 && r.wire == 0) offset = r.number;
            else if (r.field == 6 && r.wire == 2) file = r.string();
            else if (r.field == 8 && r.wire == 2) build = r.string();
        }
        return "pc 0x" + Long.toHexString(pc) + " " + file + " " + function + "+0x"
                + Long.toHexString(offset) + (build.isEmpty() ? "" : " (BuildId: " + build + ")");
    }

    private static final class Reader {
        final byte[] data;
        final int end;
        int position, field, wire, start, size;
        long number;
        Reader(byte[] data, int start, int end) { this.data = data; this.position = start; this.end = end; }
        long varint() throws IOException {
            long value = 0;
            for (int shift = 0; shift < 64; shift += 7) {
                if (position >= end) throw new IOException("Protobuf truncado");
                int b = data[position++] & 255;
                if (shift == 63 && (b & 254) != 0) throw new IOException("Varint inválido");
                value |= (long)(b & 127) << shift;
                if ((b & 128) == 0) return value;
            }
            throw new IOException("Varint demasiado largo");
        }
        boolean next() throws IOException {
            if (position == end) return false;
            long tag = varint();
            if (tag <= 0 || tag > 0xffffffffL) throw new IOException("Campo inválido");
            field = (int)(tag >>> 3); wire = (int)(tag & 7);
            if (field == 0) throw new IOException("Campo cero");
            if (wire == 0) { number = varint(); return true; }
            long length;
            if (wire == 1) length = 8;
            else if (wire == 2) length = varint();
            else if (wire == 5) length = 4;
            else throw new IOException("Tipo protobuf no admitido");
            if (length < 0 || length > end - position) throw new IOException("Longitud inválida");
            start = position; size = (int)length; position += size;
            return true;
        }
        Reader child() { return new Reader(data, start, start + size); }
        String string() { return new String(data, start, Math.min(size, 8192), StandardCharsets.UTF_8); }
    }
}
