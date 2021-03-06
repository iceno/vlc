/*****************************************************************************
 * vlc_access.h: Access descriptor, queries and methods
 *****************************************************************************
 * Copyright (C) 1999-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_ACCESS_H
#define VLC_ACCESS_H 1

#include <vlc_block.h>

/**
 * \defgroup access Access
 * \ingroup input
 * Raw input byte streams
 * @{
 * \file
 * Input byte stream modules interface
 */

enum access_query_e
{
    /* capabilities */
    ACCESS_CAN_SEEK,        /* arg1= bool*    cannot fail */
    ACCESS_CAN_FASTSEEK,    /* arg1= bool*    cannot fail */
    ACCESS_CAN_PAUSE,       /* arg1= bool*    cannot fail */
    ACCESS_CAN_CONTROL_PACE,/* arg1= bool*    cannot fail */
    ACCESS_GET_SIZE=6,      /* arg1= uin64_t* */

    /* */
    ACCESS_GET_PTS_DELAY = 0x101,/* arg1= int64_t*       cannot fail */
    ACCESS_GET_TITLE_INFO,  /* arg1=input_title_t*** arg2=int*  res=can fail */
    ACCESS_GET_TITLE,       /* arg1=unsigned * res=can fail */
    ACCESS_GET_SEEKPOINT,   /* arg1=unsigned * res=can fail */

    /* Meta data */
    ACCESS_GET_META,        /* arg1= vlc_meta_t ** res=can fail */
    ACCESS_GET_CONTENT_TYPE,/* arg1=char **ppsz_content_type res=can fail */

    ACCESS_GET_SIGNAL,      /* arg1=double *pf_quality, arg2=double *pf_strength   res=can fail */

    /* */
    ACCESS_SET_PAUSE_STATE = 0x200, /* arg1= bool           can fail */

    /* */
    ACCESS_SET_TITLE,       /* arg1= int            can fail */
    ACCESS_SET_SEEKPOINT,   /* arg1= int            can fail */

    /* Special mode for access/demux communication
     * XXX: avoid to use it unless you can't */
    ACCESS_SET_PRIVATE_ID_STATE = 0x1000, /* arg1= int i_private_data, bool b_selected    res=can fail */
    ACCESS_SET_PRIVATE_ID_CA,             /* arg1= int i_program_number, uint16_t i_vpid, uint16_t i_apid1, uint16_t i_apid2, uint16_t i_apid3, uint8_t i_length, uint8_t *p_data */
    ACCESS_GET_PRIVATE_ID_STATE,          /* arg1=int i_private_data arg2=bool *          res=can fail */
};

struct access_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t    *p_module;

    /* Access name (empty if non forced) */
    char        *psz_access;
    char        *psz_location; /**< Location (URL with the scheme stripped) */
    char        *psz_filepath; /**< Local file path (if applicable) */

    /* Access can fill this entry to force a demuxer
     * XXX: fill it once you know for sure you will succeed
     * (if you fail, this value won't be reseted */
    char        *psz_demux;

    /* pf_read/pf_block/pf_readdir is used to read data.
     * XXX A access should set one and only one of them */
    ssize_t     (*pf_read)   ( access_t *, uint8_t *, size_t );  /* Return -1 if no data yet, 0 if no more data, else real data read */
    block_t    *(*pf_block)  ( access_t * );                     /* Return a block of data in his 'natural' size, NULL if not yet data or eof */

    /* pf_readdir: Read the next input_item_t from the directory stream. It
     * returns the next input item on success or NULL in case of error or end
     * of stream. The item must be released with input_item_Release. */
    input_item_t *(*pf_readdir)( access_t * );

    /* Called for each seek.
     * XXX can be null */
    int         (*pf_seek) ( access_t *, uint64_t );         /* can be null if can't seek */

    /* Used to retreive and configure the access
     * XXX mandatory. look at access_query_e to know what query you *have to* support */
    int         (*pf_control)( access_t *, int i_query, va_list args);

    /* Access has to maintain them uptodate */
    struct
    {
        uint64_t     i_pos;     /* idem */
        bool         b_eof;     /* idem */

        bool         b_dir_sorted; /* Set it to true if items returned by
                                    * pf_readdir are already sorted. */

        bool         b_dir_can_loop;  /* Set it to true if the access can't know
                                       * if children can loop into their parents.
                                       * It's the case for most network accesses. */
    } info;
    access_sys_t *p_sys;

    /* Weak link to parent input */
    input_thread_t *p_input;
};

/**
 * Opens a new read-only byte stream.
 *
 * This function might block.
 * The initial offset is of course always zero.
 *
 * \param obj parent VLC object
 * \param mrl media resource location to read
 * \return a new access object on success, NULL on failure
 */
VLC_API access_t *vlc_access_NewMRL(vlc_object_t *obj, const char *mrl);

/**
 * Closes a byte stream.
 * \param access byte stream to close
 */
VLC_API void vlc_access_Delete(access_t *access);

/**
 * Sets the read byte offset.
 */
static inline int vlc_access_Seek(access_t *access, uint64_t offset)
{
    if (access->pf_seek == NULL)
        return VLC_EGENERIC;
    return access->pf_seek(access, offset);
}

/**
 * Gets the read byte offset.
 */
static inline uint64_t vlc_access_Tell(const access_t *access)
{
    return access->info.i_pos;
}

/**
 * Checks if end-of-stream is reached.
 */
static inline bool vlc_access_Eof(const access_t *access)
{
    return access->info.b_eof;
}

/**
 * Reads a byte stream.
 *
 * This function waits for some data to be available (if necessary) and returns
 * available data (up to the requested size). Not all byte streams support
 * this. Some streams must be read with vlc_access_Block() instead.
 *
 * \note
 * A short read does <b>not</b> imply the end of the stream. It merely implies
 * that enough data is not immediately available.
 * To detect the end of the stream, either check if the function returns zero,
 * or call vlc_access_Eof().
 *
 * \note
 * The function may return a negative value spuriously. Negative error values
 * should be ignored; they do not necessarily indicate a fatal error.
 *
 * \param buf buffer to read data into
 * \param len size of the buffer in bytes
 * \return the number of bytes read (possibly less than requested),
 *         zero at end-of-stream, or -1 on <b>transient</b> errors
  */
static inline ssize_t vlc_access_Read(access_t *access, void *buf, size_t len)
{
    if (access->pf_read == NULL)
        return -1;
    return access->pf_read(access, (unsigned char *)buf, len);
}

/**
 * Dequeues one block of data.
 *
 * This function waits for a block of data to be available (if necessary) and
 * returns a reference to it. Not all byte streams support this. Some streams
 * must be read with vlc_access_Read() instead.
 *
 * \note
 * The returned block may be of any size. The size is dependent on the
 * underlying implementation of the byte stream.
 *
 * \note
 * The function may return NULL spuriously. A NULL return is not indicative of
 * a fatal error.
 *
 * \return a data block (free with block_Release()) or NULL
 */
static inline block_t *vlc_access_Block(access_t *access)
{
    if (access->pf_block == NULL)
        return NULL;
    return access->pf_block(access);
}

static inline int access_vaControl( access_t *p_access, int i_query, va_list args )
{
    if( !p_access ) return VLC_EGENERIC;
    return p_access->pf_control( p_access, i_query, args );
}

static inline int access_Control( access_t *p_access, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = access_vaControl( p_access, i_query, args );
    va_end( args );
    return i_result;
}

static inline uint64_t access_GetSize( access_t *p_access )
{
    uint64_t val;
    if( access_Control( p_access, ACCESS_GET_SIZE, &val ) )
        val = 0;
    return val;
}

static inline void access_InitFields( access_t *p_a )
{
    p_a->info.i_pos = 0;
    p_a->info.b_eof = false;
}

/**
 * Default pf_control callback for directory accesses.
 */
VLC_API int access_vaDirectoryControlHelper( access_t *p_access, int i_query, va_list args );

#define ACCESS_SET_CALLBACKS( read, block, control, seek ) \
    do { \
        p_access->pf_read = (read); \
        p_access->pf_block = (block); \
        p_access->pf_control = (control); \
        p_access->pf_seek = (seek); \
    } while(0)

#define STANDARD_READ_ACCESS_INIT \
    do { \
        access_InitFields( p_access ); \
        ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek ); \
        p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) ); \
        if( !p_sys ) return VLC_ENOMEM;\
    } while(0);

#define STANDARD_BLOCK_ACCESS_INIT \
    do { \
        access_InitFields( p_access ); \
        ACCESS_SET_CALLBACKS( NULL, Block, Control, Seek ); \
        p_sys = p_access->p_sys = (access_sys_t*)calloc( 1, sizeof( access_sys_t ) ); \
        if( !p_sys ) return VLC_ENOMEM; \
    } while(0);

/**
 * @}
 */

#endif
