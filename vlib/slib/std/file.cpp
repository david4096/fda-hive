/*
 *  ::718604!
 * 
 * Copyright(C) November 20, 2014 U.S. Food and Drug Administration
 * Authors: Dr. Vahan Simonyan (1), Dr. Raja Mazumder (2), et al
 * Affiliation: Food and Drug Administration (1), George Washington University (2)
 * 
 * All rights Reserved.
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <utime.h>

#include <slib/std/file.hpp>

using namespace slib;

static int local_lstat(const char * flnm, struct stat * fst)
{
#ifdef WIN32
    TODO implement
#else
    sStr buf;
    idx len = sLen(flnm);
    if( !fst || !len ) {
        return -1;
    }
    // lstat interprets symlinked dir with "/" at end as the link's target
    // and we always use paths with '/' at the end, so we need to remove it
    if( flnm[len - 1] == '/' ) {
        for(; len && flnm[len - 1] == '/'; len--)
        {}
        buf.add(flnm, len);
        buf.add0();
        flnm = buf.ptr();
    }
    return lstat(flnm, fst);
#endif
}

// static
void sFile::chmod(const char * file, idx mod)
{
#ifndef SLIB_WIN
    if( !mod )
        mod = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH;
#endif

    ::chmod(file, (int) mod);
}

// static
bool sFile::symlink(const char *pathname, const char *slink)
{
    if( ::symlink(pathname, slink) == 0 )
        return true;

    // if this link already exists, we already succeeded
    if( errno == EEXIST ) {
        char buf[PATH_MAX + 1];
        idx len = readlink(slink, buf, PATH_MAX);
        if( len < 0 )
            return false;

        buf[len] = 0;
        if( strncmp(pathname, buf, len) == 0 )
            return true;
    }
    return false;
}

// static
bool sFile::exists(const char * file, bool follow_link /* = true */)
{
    struct stat buf;
    int r;
#ifdef WIN32
    r = stat(file, &buf);
    return r == 0 && (buf.st_mode & (S_IFSOCK | S_IFLNK | S_IFREG | S_IFBLK | S_IFCHR | S_IFIFO));
#else
    if( follow_link ) {
        r = stat(file, &buf);
    } else {
        r = local_lstat(file, &buf);
    }
    return r == 0 && (!follow_link || (buf.st_mode & (S_IFSOCK | S_IFLNK | S_IFREG | S_IFBLK | S_IFCHR | S_IFIFO)));
#endif
}

// static
bool sFile::isSymLink(const char * flnm)
{
#ifdef WIN32
    TODO implement
#else
    struct stat fst;
    if( local_lstat(flnm, &fst) == 0 ) {
        return S_ISLNK(fst.st_mode);
    }
    return false;
#endif
}

// static
bool sFile::sameInode(const char * file1, const char * file2, bool follow_link /* = true */)
{
    struct stat st1, st2;
    sSet(&st1);
    sSet(&st2);

    if( follow_link ) {
        sStr buf;
        // we use followSymLink instead of stat to check for equal dangling links
        followSymLink(file1, buf, &st1);
        followSymLink(file2, buf, &st2);
    } else {
#ifdef WIN32
        stat(file1, &st1);
        stat(file2, &st2);
#else
        lstat(file1, &st1);
        lstat(file2, &st2);
#endif
    }

    InodeKey k1(&st1);
    InodeKey k2(&st2);
    return k1 == k2;
}

static void resolveLinkTarget(sStr & path, const char * target, bool preferRelative)
{
    if( preferRelative || target[0] == '/' ) {
        path.cut(0);
    } else {
        // If target is relative, append it to path's directory component
        if( const char * endDirComponent = strrchr(path.ptr(), '/') ) {
            path.cut(endDirComponent + 1 - path.ptr());
        } else {
            path.cut(0);
        }
    }
    path.printf(target);
}

const char * sFile::LinkFollower::follow(const char * file)
{
#ifdef WIN32
    TODO implement
#else
    struct stat st;
    char buf[PATH_MAX + 1];
    sStr abspath;

    _path.cut(0);
    _path.printf("%s", file);
    _level = 0;
    sSet<struct stat>(_st);

    if( lstat(_path.ptr(), &st) != 0 ) {
        // path is a dangling symlink
        return _path.ptr();
    }

    *_st = st;
    bool wantRelative = true;
    abspath.printf("%s", file);

    while( S_ISLNK(_st->st_mode) ) {
        if( finished() )
            return _path.ptr();

        if( !valid() )
            return 0;

        idx readSize = readlink(_path.ptr(), buf, PATH_MAX);
        if( readSize < 0 )
            return 0;

        buf[readSize] = 0;

        // If current target is absolute, remember to return an absolute path, even if on later loop iterations target will be relative
        if( wantRelative && buf[0] == '/' )
            wantRelative = false;

        resolveLinkTarget(_path, buf, wantRelative);
        resolveLinkTarget(abspath, buf, false);

        if( lstat(abspath.ptr(), &st) != 0 ) {
            // path is a dangling symlink
            return _path.ptr();
        }

        *_st = st;
        _level++;
    }

    if( !valid() )
        return 0;

    return _path.ptr();
#endif
}

class sFileBasicFollower : public sFile::LinkFollower
{
protected:
    idx _maxLevels;

    virtual bool finished()
    {
        return _level >= _maxLevels;
    }

public:
    sFileBasicFollower(sStr & path, struct stat * st, idx maxLevels): LinkFollower(path, st), _maxLevels(maxLevels) {}
};

// static
const char * sFile::followSymLink(const char * file, sStr & outPath, struct stat * outSt /* = 0 */, idx maxLevels /* = sIdxMax */)
{
    struct stat tmp;
    sFileBasicFollower follower(outPath, outSt ? outSt : &tmp, maxLevels);
    return follower.follow(file);
}

// static
bool sFile::remove(const char * file)
{
    return ::remove(file) == 0;
}

// static
bool sFile::copy(const char * filesrc, const char * filedst, bool doAppend /* = false */, bool follow_link /* = true */)
{
    if( sameInode(filesrc, filedst, follow_link) )
        return false;

    struct stat st;

    if( !follow_link && isSymLink(filesrc) ) {
        sStr buf;
        const char * target = followSymLink(filesrc, buf, &st, 1);
        if( !target )
            return false;

        if( sFile::exists(filedst, false) && !sFile::remove(filedst) )
            return false;

        return sFile::symlink(target, filedst);
    }

    bool ok = false;
    idx fin = open(filesrc, O_RDONLY, S_IREAD);
    if( fin >= 0 ) {
        idx fout = open(filedst, O_WRONLY | O_CREAT | (doAppend ? O_APPEND : 0), S_IREAD | S_IWRITE);
        if( fout >= 0 ) {
            static const idx size = 4 * 1024 * 1024;
            char buf[size];
            idx len = 0;
            errno = 0;
            while( (len = read(fin, buf, size)) != 0 && errno == 0 ) {
                idx written = 0;
                while( len - written > 0 && errno == 0 ) {
                    written += write(fout, buf + written, len - written);
                }
            }
            ok = errno == 0;
            if( fstat(fin, &st) == 0 ) {
                // ignore failures in chown, chmod etc. since some destination filesystems may not allow them
                sFile::fsetAttributes(fout, &st);
            } else {
                ok = false;
            }
            ok |= close(fout) == 0;
        }
        close(fin);
    }
    return ok;
}

// static
bool sFile::rename(const char * filesrc, const char * filedst)
{
    idx rs = ::rename(filesrc, filedst);
    if( rs != 0 ) {
        if( copy(filesrc, filedst, false, false) ) {
            remove(filesrc);
            return true;
        }
        return false;
    }
    return true;
}

// static
idx sFile::time(const char * flnm, bool follow_link /* = true */)
{
    struct stat fst;
    sSet(&fst, 0);
    return (follow_link ? stat(flnm, &fst) : local_lstat(flnm, &fst)) == -1 ? sIdxMax : (idx) fst.st_mtime;
}

// static
idx sFile::atime(const char * flnm, bool follow_link /* = true */)
{
    struct stat fst;
    sSet(&fst, 0);
    return (follow_link ? stat(flnm, &fst) : local_lstat(flnm, &fst)) == -1 ? sIdxMax : (idx) fst.st_atime;
}

// static
bool sFile::touch(const char * file, idx acc_and_mod_time /* = 0 */)
{
    struct utimbuf t;
    if( file ) {
        t.actime = acc_and_mod_time ? acc_and_mod_time : time(0);
        t.modtime = acc_and_mod_time ? acc_and_mod_time : t.actime;
    }
    return file ? utime(file, &t) == 0 : false;
}

// static
bool sFile::setAttributes(const char * file, const struct stat * st)
{
    bool ret = true;
    if( ::chown(file, st->st_uid, st->st_gid) != 0) {
        ret = false;
    }
    if( ::chmod(file, st->st_mode) != 0 ) {
        ret = false;
    }
#if _XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L || defined(_ATFILE_SOURCE)
    struct timespec times[2];
    sSetArray(times);
    times[0].tv_sec = st->st_atime;
    times[1].tv_sec = st->st_mtime;
    times[0].tv_nsec = st->st_atim.tv_nsec;
    times[1].tv_nsec = st->st_mtim.tv_nsec;
    if( utimensat(AT_FDCWD, file, times, 0) != 0 ) {
        ret = false;
    }
#else
    struct timeval times[2];
    times[0].tv_sec = st->st_atime;
    times[1].tv_sec = st->st_mtime;
    #if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE)
        times[0].tv_usec = st->st_atim.tv_nsec / 1000;
        times[1].tv_usec = st->st_mtim.tv_nsec / 1000;
    #else
        times[0].tv_usec = 0;
        times[1].tv_usec = 0;
    #endif
    if( utimes(file, times) != 0 ) {
        ret = false;
    }
#endif
    return ret;
}

// static
bool sFile::fsetAttributes(int fd, const struct stat * st)
{
    bool ret = true;
    if( fchown(fd, st->st_uid, st->st_gid) != 0) {
        ret = false;
    }
    if( fchmod(fd, st->st_mode) != 0 ) {
        ret = false;
    }
#if _XOPEN_SOURCE >= 700 || _POSIX_C_SOURCE >= 200809L || defined(_GNU_SOURCE)
    struct timespec times[2];
    sSetArray(times);
    times[0].tv_sec = st->st_atime;
    times[1].tv_sec = st->st_mtime;
    times[0].tv_nsec = st->st_atim.tv_nsec;
    times[1].tv_nsec = st->st_mtim.tv_nsec;
    if( futimens(fd, times) != 0 ) {
        ret = false;
    }
#else
    struct timeval times[2];
    times[0].tv_sec = st->st_atime;
    times[1].tv_sec = st->st_mtime;
    #if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE)
        times[0].tv_usec = st->st_atim.tv_nsec / 1000;
        times[1].tv_usec = st->st_mtim.tv_nsec / 1000;
    #else
        times[0].tv_usec = 0;
        times[1].tv_usec = 0;
    #endif
    if( futimes(fout, times) != 0 ) {
        ret = false;
    }
#endif
    return ret;
}

// static
idx sFile::size(const char * flnm, bool follow_link /* = true */)
{
    struct stat fst;
    int r;
#ifdef WIN32
    r = stat(flnm, &fst);
#else
    if( follow_link ) {
        r = stat(flnm, &fst);
    } else {
        r = local_lstat(flnm, &fst);
    }
#endif
    return r == 0 ? fst.st_size : 0;
}

static bool defaultTempPathTester(const char * path)
{
    struct stat st;

    if( stat(path, &st) == 0 )
        return false;

    FILE * f = fopen(path, "w");
    if( !f )
        return false;

    fclose(f);

    return true;
}

// static
const char * sFile::mktemp(sStr & outPath, const char * dir, const char * extension/*=0*/, const char * pattern/*=0*/, tempPathTester tester/*=0*/)
{
    if( !pattern )
        pattern = DEFAULT_MKTEMP_PATTERN;

    if( !tester )
        tester = defaultTempPathTester;

    const char * xxx = strstr(pattern, "XXXXXX");

    if( !xxx ) {
#ifdef _DEBUG
        fprintf(stderr, "%s:%u: DEVELOPER WARNING: sFile::mktemp() requires pattern with \"XXXXXX\" substring\n", __FILE__, __LINE__);
#endif
        return 0;
    }

    if( !sDir::exists(dir) && !sDir::makeDir(dir) )
        return 0;

    outPath.cut(0);
    outPath.printf("%s/", dir);
    idx filestart = outPath.length();

    for( idx i=0; i<8; i++ ) {
        outPath.cut(filestart);
        outPath.printf("%.*s%"DEC"%s", static_cast<int>(xxx - pattern), pattern, static_cast<idx>(getpid()) + rand(), xxx + 6);
        if( extension )
            outPath.printf(".%s", extension);

        if( !tester(outPath.ptr()) )
            continue;

        return outPath.ptr();
    }

    return 0;
}
