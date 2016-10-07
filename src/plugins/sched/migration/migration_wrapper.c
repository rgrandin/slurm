/*****************************************************************************\
 *  migration_wrapper.c - plugin for SLURM migration scheduler.
 *  Operates like FIFO, but migration scheduler daemon will explicitly modify
 *  the priority of jobs as needed to achieve migration scheduling.
 *****************************************************************************
 *  Copyright (C) 2003 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jay Windley <jwindley@lnxi.com>, Morris Jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "slurm/slurm_errno.h"

#include "src/common/plugin.h"
#include "src/common/log.h"
#include "src/common/slurm_priority.h"
#include "src/common/macros.h"
#include "src/slurmctld/slurmctld.h"
#include "migration.h"

const char		plugin_name[]	= "SLURM Migration Scheduler plugin";
const char		plugin_type[]	= "sched/migration";
const uint32_t		plugin_version	= SLURM_VERSION_NUMBER;

/* A plugin-global errno. */
static int plugin_errno = SLURM_SUCCESS;

static pthread_t migration_thread = 0;
static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

int init( void )
{
	pthread_attr_t attr;

	if (slurmctld_config.scheduling_disabled)
		return SLURM_SUCCESS;


	verbose( "sched: Migration scheduler plugin loaded" );

	slurm_mutex_lock( &thread_flag_mutex );
	if ( migration_thread ) {
		debug2( "Migration thread already running, not starting "
			"another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}

	slurm_attr_init( &attr );
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &migration_thread, &attr, migration_agent, NULL))
		error("Unable to start migration thread: %m");
	slurm_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr );

	return SLURM_SUCCESS;
}

void fini( void )
{
	slurm_mutex_lock( &thread_flag_mutex );
	if ( migration_thread ) {
		verbose( "Migration scheduler plugin shutting down" );
		stop_migration_agent();
		pthread_join(migration_thread, NULL);
		migration_thread = 0;
	}
	slurm_mutex_unlock( &thread_flag_mutex );
}

int slurm_sched_p_reconfig( void )
{
	migration_reconfig();
	return SLURM_SUCCESS;
}

int slurm_sched_p_schedule(void)
{
	return SLURM_SUCCESS;
}

int slurm_sched_p_newalloc(struct job_record *job_ptr)
{
	return SLURM_SUCCESS;
}

int slurm_sched_p_freealloc(struct job_record *job_ptr)
{
	return SLURM_SUCCESS;
}

uint32_t slurm_sched_p_initial_priority(uint32_t last_prio,
					struct job_record *job_ptr)
{
	return priority_g_set(last_prio, job_ptr);
}

void slurm_sched_p_job_is_pending( void )
{
	/* Empty. */
}

void slurm_sched_p_partition_change( void )
{
	/* Empty. */
}

int slurm_sched_p_get_errno( void )
{
	return plugin_errno;
}

char *slurm_sched_p_strerror( int errnum )
{
	return NULL;
}

void slurm_sched_p_requeue( struct job_record *job_ptr, char *reason )
{
	/* Empty. */
}

char *slurm_sched_p_get_conf( void )
{
	return NULL;
}
