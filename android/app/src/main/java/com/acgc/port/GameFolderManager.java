package com.acgc.port;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.system.ErrnoException;
import android.system.Os;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Owns the Android Storage Access Framework folder selected by the user.
 *
 * The GameCube disc stays in the selected folder and is exposed to native code
 * through its real seekable file descriptor. Small directory-based resources are
 * mirrored into app-private storage because the native PC port uses normal
 * fopen/opendir calls. Saves and configuration are synchronized back to the
 * selected folder so they remain visible and easy to back up.
 */
final class GameFolderManager {
    static final int TREE_REQUEST_CODE = 4101;

    private static final String PREFS = "acgc_game_folder";
    private static final String PREF_TREE_URI = "tree_uri";
    private static final String DIRECTORY_MIME = DocumentsContract.Document.MIME_TYPE_DIR;
    private static final String[] ROM_EXTENSIONS = {".iso", ".gcm", ".ciso"};

    private GameFolderManager() {
    }

    static Intent createFolderPickerIntent(Uri initialUri) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        if (initialUri != null && android.os.Build.VERSION.SDK_INT >= 26) {
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, initialUri);
        }
        return intent;
    }

    static boolean persistSelection(Context context, Uri treeUri, int resultFlags) {
        int flags = resultFlags & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        int required = Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION;
        if ((flags & required) != required) return false;
        try {
            context.getContentResolver().takePersistableUriPermission(treeUri, flags);
        } catch (SecurityException error) {
            return false;
        }

        Uri previous = getSelectedTree(context);
        if (previous != null && !previous.equals(treeUri)) {
            RuntimeSession.close();
            releasePermission(context, previous);
            clearPrivateRuntime(context);
        }
        preferences(context).edit().putString(PREF_TREE_URI, treeUri.toString()).apply();
        return true;
    }

    static Uri getSelectedTree(Context context) {
        String value = preferences(context).getString(PREF_TREE_URI, null);
        if (value == null || value.isEmpty()) return null;
        try {
            return Uri.parse(value);
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    static void forgetSelection(Context context) {
        RuntimeSession.close();
        Uri tree = getSelectedTree(context);
        if (tree != null) releasePermission(context, tree);
        preferences(context).edit().remove(PREF_TREE_URI).apply();
        clearPrivateRuntime(context);
    }

    private static void releasePermission(Context context, Uri tree) {
        try {
            context.getContentResolver().releasePersistableUriPermission(
                    tree,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (SecurityException ignored) {
        }
    }

    private static void clearPrivateRuntime(Context context) {
        File root = context.getFilesDir();
        deleteLocalTree(new File(root, "texture_pack"));
        deleteLocalTree(new File(root, "nes_roms"));
        deleteLocalTree(new File(root, "save"));
        deleteLocalTree(new File(root, "languages"));
        new File(root, "settings.ini").delete();
        new File(root, "keybindings.ini").delete();
    }

    private static void deleteLocalTree(File file) {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) deleteLocalTree(child);
            }
        }
        file.delete();
    }

    static synchronized PreparedGame prepareRuntime(Context context) throws IOException {
        Uri tree = getSelectedTree(context);
        if (tree == null) throw new IOException("Selecciona una carpeta para el juego.");

        Layout layout = ensureLayout(context, tree);
        File runtimeRoot = context.getFilesDir();
        File localTextures = new File(runtimeRoot, "texture_pack");
        File localNes = new File(runtimeRoot, "nes_roms");
        File localSaves = new File(runtimeRoot, "save");
        File localLanguages = new File(runtimeRoot, "languages");

        ensureLocalDirectory(localTextures);
        ensureLocalDirectory(localNes);
        ensureLocalDirectory(localSaves);
        ensureLocalDirectory(localLanguages);

        // User-visible folders are the source of truth for replaceable content.
        mirrorDirectoryToLocal(context, layout.textures, localTextures, ".dds", true);
        mirrorDirectoryToLocal(context, layout.nes, localNes, ".nes", false);
        mirrorDirectoryToLocal(context, layout.languages, localLanguages, null, true);

        // Saves and config are two-way: whichever copy is newer wins.
        pullDirectoryIfNewer(context, layout.saves, localSaves);
        pushDirectoryIfNewer(context, localSaves, layout.saves);
        syncConfigTwoWay(context, layout.config);
        installShaders(context);

        OpenRom rom = openCompatibleMainRom(context, layout.roms);
        RuntimeSession.set(rom.descriptor, rom.info.name);
        return new PreparedGame(rom.info.name, RuntimeSession.romFd());
    }

    static synchronized void syncOutputs(Context context) throws IOException {
        Uri tree = getSelectedTree(context);
        if (tree == null) return;
        Layout layout = ensureLayout(context, tree);
        File runtimeRoot = context.getFilesDir();
        pushDirectoryIfNewer(context, new File(runtimeRoot, "save"), layout.saves);
        pushConfig(context, layout.config);
    }

    static String selectedFolderLabel(Context context) {
        Uri tree = getSelectedTree(context);
        if (tree == null) return "";
        try {
            Uri root = rootDocumentUri(tree);
            DocInfo info = queryDocument(context, root);
            return info == null ? "Carpeta del juego" : info.name;
        } catch (Exception ignored) {
            return "Carpeta del juego";
        }
    }

    private static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    private static Layout ensureLayout(Context context, Uri tree) throws IOException {
        Uri root = rootDocumentUri(tree);
        // Querying the root also verifies that the persisted permission still works.
        if (queryDocument(context, root) == null) {
            throw new IOException("Android perdió el acceso a la carpeta seleccionada.");
        }

        Uri roms = ensureDirectory(context, root, "roms");
        Uri nes = ensureDirectory(context, roms, "nes");
        Uri textures = ensureDirectory(context, root, "textures");
        Uri languages = ensureDirectory(context, root, "languages");
        Uri saves = ensureDirectory(context, root, "saves");
        Uri config = ensureDirectory(context, root, "config");
        Uri cache = ensureDirectory(context, root, "cache");
        return new Layout(root, roms, nes, textures, languages, saves, config, cache);
    }

    private static Uri rootDocumentUri(Uri treeUri) {
        return DocumentsContract.buildDocumentUriUsingTree(
                treeUri, DocumentsContract.getTreeDocumentId(treeUri));
    }

    private static Uri ensureDirectory(Context context, Uri parent, String name) throws IOException {
        DocInfo existing = findChild(context, parent, name);
        if (existing != null) {
            if (!existing.isDirectory()) {
                throw new IOException(name + " existe, pero no es una carpeta.");
            }
            return existing.uri;
        }
        try {
            Uri created = DocumentsContract.createDocument(
                    context.getContentResolver(), parent, DIRECTORY_MIME, name);
            if (created == null) throw new IOException("No se pudo crear " + name + "/.");
            return created;
        } catch (Exception error) {
            throw new IOException("No se pudo crear " + name + "/.", error);
        }
    }

    private static OpenRom openCompatibleMainRom(Context context, Uri romsDirectory)
            throws IOException {
        IOException lastError = null;
        boolean foundCandidate = false;

        for (DocInfo child : listChildren(context, romsDirectory)) {
            if (child.isDirectory()) continue;
            String lower = child.name.toLowerCase(Locale.ROOT);
            boolean supported = false;
            for (String extension : ROM_EXTENSIONS) {
                if (lower.endsWith(extension)) {
                    supported = true;
                    break;
                }
            }
            if (!supported) continue;
            foundCandidate = true;

            ParcelFileDescriptor descriptor = context.getContentResolver()
                    .openFileDescriptor(child.uri, "r");
            if (descriptor == null) {
                lastError = new IOException("Android no pudo abrir " + child.name + ".");
                continue;
            }

            try {
                validateRom(descriptor, child.name);
                return new OpenRom(child, descriptor);
            } catch (IOException error) {
                lastError = error;
                try {
                    descriptor.close();
                } catch (IOException ignored) {
                }
            }
        }

        if (!foundCandidate) {
            throw new IOException(
                    "Coloca Animal Crossing USA Rev 0 (.iso, .gcm o .ciso) dentro de roms/.");
        }
        throw new IOException("No se encontró una ROM compatible en roms/. "
                + (lastError == null ? "" : lastError.getMessage()));
    }

    private static void validateRom(ParcelFileDescriptor descriptor, String name) throws IOException {
        long base = name.toLowerCase(Locale.ROOT).endsWith(".ciso") ? 0x8000L : 0L;
        byte[] header = new byte[0x20];
        try {
            if (base != 0L) {
                byte[] ciso = new byte[4];
                preadFully(descriptor, ciso, 0L);
                if (ciso[0] != 'C' || ciso[1] != 'I' || ciso[2] != 'S' || ciso[3] != 'O') {
                    throw new IOException("El archivo .ciso no tiene una cabecera válida.");
                }
            }
            preadFully(descriptor, header, base);
        } catch (ErrnoException error) {
            throw new IOException("La ROM debe estar en una carpeta local que permita lectura aleatoria.", error);
        }

        String id;
        try {
            id = new String(header, 0, 6, "US-ASCII");
        } catch (Exception impossible) {
            id = "";
        }
        if (!"GAFE01".equals(id) || header[6] != 0 || header[7] != 0) {
            throw new IOException("ROM incompatible: se necesita Animal Crossing USA Rev 0 (GAFE01_00).");
        }
        if ((header[0x1c] & 0xff) != 0xc2 || (header[0x1d] & 0xff) != 0x33
                || (header[0x1e] & 0xff) != 0x9f || (header[0x1f] & 0xff) != 0x3d) {
            throw new IOException("La ROM no parece ser una imagen válida de GameCube.");
        }
    }

    private static void preadFully(ParcelFileDescriptor descriptor, byte[] output, long offset)
            throws ErrnoException, IOException {
        int done = 0;
        while (done < output.length) {
            int read = Os.pread(descriptor.getFileDescriptor(), output, done,
                    output.length - done, offset + done);
            if (read <= 0) throw new IOException("La ROM terminó antes de lo esperado.");
            done += read;
        }
    }

    private static void installShaders(Context context) throws IOException {
        File shaders = new File(context.getFilesDir(), "shaders");
        ensureLocalDirectory(shaders);
        copyAsset(context, "default.vert", new File(shaders, "default.vert"));
        copyAsset(context, "default.frag", new File(shaders, "default.frag"));
    }

    private static void copyAsset(Context context, String name, File target) throws IOException {
        try (InputStream input = context.getAssets().open(name);
             OutputStream output = new FileOutputStream(target)) {
            copy(input, output);
        }
    }

    private static void mirrorDirectoryToLocal(Context context, Uri source, File target,
                                               String extension, boolean recursive) throws IOException {
        ensureLocalDirectory(target);
        Set<String> expected = new HashSet<>();
        boolean changed = mirrorChildren(context, source, target, "", extension, recursive, expected);
        changed |= deleteUnexpectedLocal(target, target, expected,
                ".dds".equals(extension) ? "texture_cache.bin" : null);
        if (changed && ".dds".equals(extension)) {
            // The native binary cache only validates the number of entries.
            // Remove it when any source texture changes, even if the count did not.
            new File(target, "texture_cache.bin").delete();
        }
    }

    private static boolean mirrorChildren(Context context, Uri source, File localRoot, String relative,
                                          String extension, boolean recursive, Set<String> expected)
            throws IOException {
        boolean changed = false;
        for (DocInfo child : listChildren(context, source)) {
            String childRelative = relative.isEmpty() ? child.name : relative + "/" + child.name;
            if (child.isDirectory()) {
                if (recursive) {
                    File localDir = new File(localRoot, childRelative);
                    ensureLocalDirectory(localDir);
                    changed |= mirrorChildren(context, child.uri, localRoot, childRelative,
                            extension, true, expected);
                }
                continue;
            }
            if (extension != null &&
                    !child.name.toLowerCase(Locale.ROOT).endsWith(extension)) continue;
            String localRelative = extension == null
                    ? childRelative : normalizeExtension(childRelative, extension);
            expected.add(localRelative);
            File localFile = new File(localRoot, localRelative);
            File parent = localFile.getParentFile();
            if (parent != null) ensureLocalDirectory(parent);
            if (!sameEnough(localFile, child)) {
                copyDocumentToFile(context, child, localFile);
                changed = true;
            }
        }
        return changed;
    }

    private static String normalizeExtension(String name, String extension) {
        if (name.length() < extension.length()) return name;
        return name.substring(0, name.length() - extension.length()) + extension;
    }

    private static boolean sameEnough(File local, DocInfo remote) {
        if (!local.isFile()) return false;
        if (remote.size >= 0 && local.length() != remote.size) return false;
        if (remote.modified <= 0) return false;
        return local.lastModified() + 1500L >= remote.modified;
    }

    private static boolean deleteUnexpectedLocal(File root, File current, Set<String> expected,
                                                 String preservedName) {
        boolean changed = false;
        File[] children = current.listFiles();
        if (children == null) return false;
        for (File child : children) {
            if (preservedName != null && child.getName().equals(preservedName)) continue;
            if (child.isDirectory()) {
                changed |= deleteUnexpectedLocal(root, child, expected, preservedName);
                File[] remaining = child.listFiles();
                if (remaining != null && remaining.length == 0 && child.delete()) changed = true;
            } else {
                String relative = relativePath(root, child);
                if (!expected.contains(relative) && child.delete()) changed = true;
            }
        }
        return changed;
    }

    private static void pullDirectoryIfNewer(Context context, Uri source, File target)
            throws IOException {
        ensureLocalDirectory(target);
        for (DocInfo child : listChildren(context, source)) {
            File local = new File(target, child.name);
            if (child.isDirectory()) {
                ensureLocalDirectory(local);
                pullDirectoryIfNewer(context, child.uri, local);
            } else if (!local.exists() || child.modified <= 0
                    || local.lastModified() + 1500L < child.modified
                    || local.length() != child.size) {
                copyDocumentToFile(context, child, local);
            }
        }
    }

    private static void pushDirectoryIfNewer(Context context, File source, Uri target)
            throws IOException {
        if (!source.isDirectory()) return;
        File[] files = source.listFiles();
        if (files == null) return;
        for (File file : files) {
            if (file.getName().startsWith(".")) continue;
            DocInfo remote = findChild(context, target, file.getName());
            if (file.isDirectory()) {
                Uri remoteDir = remote == null
                        ? ensureDirectory(context, target, file.getName())
                        : remote.uri;
                if (remote != null && !remote.isDirectory()) continue;
                pushDirectoryIfNewer(context, file, remoteDir);
            } else if (remote == null || remote.isDirectory()
                    || remote.modified <= 0
                    || file.lastModified() > remote.modified + 1500L
                    || file.length() != remote.size) {
                copyFileToDocument(context, file, target, remote);
            }
        }
    }

    private static void syncConfigTwoWay(Context context, Uri configDirectory) throws IOException {
        String[] names = {"settings.ini", "keybindings.ini"};
        for (String name : names) {
            File local = new File(context.getFilesDir(), name);
            DocInfo remote = findChild(context, configDirectory, name);
            if (remote != null && !remote.isDirectory()
                    && (!local.exists() || remote.modified <= 0
                    || local.lastModified() + 1500L < remote.modified
                    || local.length() != remote.size)) {
                copyDocumentToFile(context, remote, local);
            }
            if (local.isFile()) {
                remote = findChild(context, configDirectory, name);
                if (remote == null || remote.modified <= 0
                        || local.lastModified() > remote.modified + 1500L
                        || local.length() != remote.size) {
                    copyFileToDocument(context, local, configDirectory, remote);
                }
            }
        }
    }

    private static void pushConfig(Context context, Uri configDirectory) throws IOException {
        String[] names = {"settings.ini", "keybindings.ini"};
        for (String name : names) {
            File local = new File(context.getFilesDir(), name);
            if (!local.isFile()) continue;
            DocInfo remote = findChild(context, configDirectory, name);
            if (remote == null || remote.modified <= 0
                    || local.lastModified() > remote.modified + 1500L
                    || local.length() != remote.size) {
                copyFileToDocument(context, local, configDirectory, remote);
            }
        }
    }

    private static void copyDocumentToFile(Context context, DocInfo source, File target)
            throws IOException {
        File parent = target.getParentFile();
        if (parent != null) ensureLocalDirectory(parent);
        File temporary = new File(target.getPath() + ".incoming");
        try (InputStream input = context.getContentResolver().openInputStream(source.uri);
             OutputStream output = new FileOutputStream(temporary)) {
            if (input == null) throw new IOException("No se pudo leer " + source.name + ".");
            copy(input, output);
        }
        if (target.exists() && !target.delete()) {
            temporary.delete();
            throw new IOException("No se pudo reemplazar " + target.getName() + ".");
        }
        if (!temporary.renameTo(target)) {
            try (InputStream input = new FileInputStream(temporary);
                 OutputStream output = new FileOutputStream(target)) {
                copy(input, output);
            }
            temporary.delete();
        }
        if (source.modified > 0) target.setLastModified(source.modified);
    }

    private static void copyFileToDocument(Context context, File source, Uri parent, DocInfo existing)
            throws IOException {
        ContentResolver resolver = context.getContentResolver();
        Uri target = existing == null || existing.isDirectory() ? null : existing.uri;
        try {
            if (target == null) {
                target = DocumentsContract.createDocument(
                        resolver, parent, "application/octet-stream", source.getName());
            }
            if (target == null) throw new IOException("No se pudo crear " + source.getName() + ".");
            try (InputStream input = new FileInputStream(source);
                 OutputStream output = resolver.openOutputStream(target, "wt")) {
                if (output == null) throw new IOException("No se pudo escribir " + source.getName() + ".");
                copy(input, output);
            }
        } catch (Exception error) {
            if (error instanceof IOException) throw (IOException) error;
            throw new IOException("No se pudo guardar " + source.getName() + ".", error);
        }
    }

    private static void copy(InputStream input, OutputStream output) throws IOException {
        byte[] buffer = new byte[1024 * 1024];
        int read;
        while ((read = input.read(buffer)) >= 0) {
            if (read > 0) output.write(buffer, 0, read);
        }
        output.flush();
    }

    private static void ensureLocalDirectory(File directory) throws IOException {
        if (!directory.exists() && !directory.mkdirs()) {
            throw new IOException("No se pudo crear " + directory.getName() + "/.");
        }
        if (!directory.isDirectory()) {
            throw new IOException(directory.getName() + " no es una carpeta.");
        }
    }

    private static String relativePath(File root, File file) {
        String base = root.getAbsolutePath();
        String path = file.getAbsolutePath();
        if (path.startsWith(base)) {
            path = path.substring(base.length());
            while (path.startsWith(File.separator)) path = path.substring(1);
        }
        return path.replace(File.separatorChar, '/');
    }

    private static DocInfo queryDocument(Context context, Uri document) throws IOException {
        String[] projection = {
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_LAST_MODIFIED
        };
        try (Cursor cursor = context.getContentResolver().query(document, projection,
                null, null, null)) {
            if (cursor == null || !cursor.moveToFirst()) return null;
            return readDocInfo(document, cursor);
        } catch (Exception error) {
            throw new IOException("No se pudo leer la carpeta seleccionada.", error);
        }
    }

    private static DocInfo findChild(Context context, Uri parent, String name) throws IOException {
        for (DocInfo child : listChildren(context, parent)) {
            if (child.name.equals(name)) return child;
        }
        return null;
    }

    private static List<DocInfo> listChildren(Context context, Uri parent) throws IOException {
        List<DocInfo> result = new ArrayList<>();
        String parentId = DocumentsContract.getDocumentId(parent);
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(parent, parentId);
        String[] projection = {
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE,
                DocumentsContract.Document.COLUMN_LAST_MODIFIED
        };
        try (Cursor cursor = context.getContentResolver().query(children, projection,
                null, null, null)) {
            if (cursor == null) return result;
            while (cursor.moveToNext()) {
                String documentId = cursor.getString(0);
                Uri uri = DocumentsContract.buildDocumentUriUsingTree(parent, documentId);
                result.add(readDocInfo(uri, cursor));
            }
        } catch (Exception error) {
            throw new IOException("No se pudo listar la carpeta del juego.", error);
        }
        return result;
    }

    private static DocInfo readDocInfo(Uri uri, Cursor cursor) {
        String name = cursor.getString(1);
        String mime = cursor.getString(2);
        long size = cursor.isNull(3) ? -1L : cursor.getLong(3);
        long modified = cursor.isNull(4) ? 0L : cursor.getLong(4);
        return new DocInfo(uri, name == null ? "archivo" : name, mime, size, modified);
    }

    static final class PreparedGame {
        final String romName;
        final int romFd;

        PreparedGame(String romName, int romFd) {
            this.romName = romName;
            this.romFd = romFd;
        }
    }

    static final class RuntimeSession {
        private static ParcelFileDescriptor descriptor;
        private static String romName;

        private RuntimeSession() {
        }

        static synchronized void set(ParcelFileDescriptor value, String name) {
            close();
            descriptor = value;
            romName = name;
        }

        static synchronized int romFd() {
            return descriptor == null ? -1 : descriptor.getFd();
        }

        static synchronized String romName() {
            return romName;
        }

        static synchronized boolean isReady() {
            return descriptor != null;
        }

        static synchronized void close() {
            if (descriptor != null) {
                try {
                    descriptor.close();
                } catch (IOException ignored) {
                }
            }
            descriptor = null;
            romName = null;
        }
    }

    private static final class OpenRom {
        final DocInfo info;
        final ParcelFileDescriptor descriptor;

        OpenRom(DocInfo info, ParcelFileDescriptor descriptor) {
            this.info = info;
            this.descriptor = descriptor;
        }
    }

    private static final class Layout {
        final Uri root;
        final Uri roms;
        final Uri nes;
        final Uri textures;
        final Uri languages;
        final Uri saves;
        final Uri config;
        final Uri cache;

        Layout(Uri root, Uri roms, Uri nes, Uri textures, Uri languages, Uri saves, Uri config, Uri cache) {
            this.root = root;
            this.roms = roms;
            this.nes = nes;
            this.textures = textures;
            this.languages = languages;
            this.saves = saves;
            this.config = config;
            this.cache = cache;
        }
    }

    private static final class DocInfo {
        final Uri uri;
        final String name;
        final String mime;
        final long size;
        final long modified;

        DocInfo(Uri uri, String name, String mime, long size, long modified) {
            this.uri = uri;
            this.name = name;
            this.mime = mime;
            this.size = size;
            this.modified = modified;
        }

        boolean isDirectory() {
            return DIRECTORY_MIME.equals(mime);
        }
    }
}
