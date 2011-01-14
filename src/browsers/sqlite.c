/**
 * This file is part of Slideshow.
 * Copyright (C) 2008-2010 David Sveningsson <ext@sidvind.com>
 *
 * Slideshow is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Slideshow is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Slideshow.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "module.h"
#include "Browser.h"
#include <sqlite3.h>
#include <string.h>

typedef struct {
	browser_context_t base;

	int loop_queue;
	int queue_id;
	int prev_slide_id;

	sqlite3* conn;
	sqlite3_stmt* query_slide;
	sqlite3_stmt* query_looping;
	sqlite3_stmt* query_pop_intermediate;
} sqlite3_context_t;

MODULE_INFO("SQLite3 Browser", BROWSER_MODULE, "David Sveningsson");
MODULE_CONTEXT(sqlite3_context_t);

static int connect(sqlite3_context_t* this){
	int ret;
	if ( (ret = sqlite3_open(this->base.name, &this->conn)) != SQLITE_OK ){
		//Log::message(Log::Fatal, "sqlite3_open failed with %d\n", ret);
		return ret;
	}

	const char* q =
		"	SELECT" /* select from intermediate queue */
		"		id, "
		"		path, "
		"		sortorder, "
		"		queue_id, "
		"		assembler "
		"	FROM"
		"		slide "
		"	WHERE"
		"		queue_id = -1 " /* -1 is intermediate queue */
		"UNION "
		"	SELECT" /* select next slide from regular queue */
		"		id, "
		"		path, "
		"		sortorder, "
		"		queue_id, "
		"		assembler "
		"	FROM"
		"		slide "
		"	WHERE"
		"		queue_id = ? AND "
		"		sortorder > ? "
		"ORDER BY"
		"	queue_id, "
		"	sortorder "
		"LIMIT 1";
	if ( (ret = sqlite3_prepare_v2(this->conn, q, (int)(strlen(q)+1), &this->query_slide, NULL)) != SQLITE_OK ){
		//Log::message(Log::Fatal, "query_slide::sqlite3_prepare_v2 failed with %d\n", ret);
		return ret;
	}

	q =
		"SELECT "
		"	loop "
		"FROM "
		"	queue "
		"WHERE "
		"	id = ? "
		"LIMIT 1";
	if ( (ret = sqlite3_prepare_v2(this->conn, q, (int)(strlen(q)+1), &this->query_looping, NULL)) != SQLITE_OK ){
		//Log::message(Log::Fatal, "query_loop::sqlite3_prepare_v2 failed with %d\n", ret);
		return ret;
	}

	q =
		"UPDATE "
		"	slide "
		"SET "
		"	queue_id = 0 "
		"WHERE "
		"	id = ?";
	if ( (ret = sqlite3_prepare_v2(this->conn, q, (int)(strlen(q)+1), &this->query_pop_intermediate, NULL)) != SQLITE_OK ){
		//Log::message(Log::Fatal, "pop_intermediate::sqlite3_prepare_v2 failed with %d\n", ret);
		return ret;
	}
}

static int disconnect(sqlite3_context_t* this){
	sqlite3_finalize(this->query_pop_intermediate);
	sqlite3_finalize(this->query_looping);
	sqlite3_finalize(this->query_slide);
	sqlite3_close(this->conn);
	return 0;
}

static int pop_intermediate(sqlite3_context_t* this, int id){
	sqlite3_bind_int(this->query_pop_intermediate, 1, id);
	int ret = sqlite3_step(this->query_pop_intermediate);
	sqlite3_reset(this->query_pop_intermediate);

	switch ( ret ){
		case SQLITE_DONE:
		case SQLITE_ROW:
			return 0;
		case SQLITE_MISUSE:
			//Log::message(Log::Info, "pop_intermediate::sqlite3_step failed: SQLITE_MISUSE\n");
			break;
		default:
			//Log::message(Log::Info, "pop_intermediate::sqlite3_step failed: %d\n", ret);
			break;
	}

	return ret;
}

int EXPORT module_init(sqlite3_context_t* this){
	this->loop_queue = 1;
	this->queue_id = 0;
	this->prev_slide_id = -1;
	this->conn = 0;
	this->query_slide  = 0;
	this->query_looping  = 0;
	this->query_pop_intermediate  = 0;

	return connect(this);
}

int EXPORT module_cleanup(sqlite3_context_t* this){
	return disconnect(this);
}

slide_context_t EXPORT next_slide(sqlite3_context_t* this){
	slide_context_t slide;
	slide.filename = NULL;
	slide.assembler = NULL;

	sqlite3_bind_int(this->query_slide, 1, this->queue_id);
	sqlite3_bind_int(this->query_slide, 2, this->prev_slide_id);

	int ret = sqlite3_step(this->query_slide);
	if ( ret == SQLITE_DONE ){
		sqlite3_reset(this->query_slide);

		if ( !this->loop_queue ){
			//Log::message(Log::Debug, "queue finished\n");
			return slide;
		}

		//Log::message(Log::Debug, "queue wrapping\n");
		this->prev_slide_id = -1;
		sqlite3_bind_int(this->query_slide, 1, this->queue_id);
		sqlite3_bind_int(this->query_slide, 2, this->prev_slide_id);

		ret = sqlite3_step(this->query_slide);
		if ( ret == SQLITE_DONE ){
			sqlite3_reset(this->query_slide);
			return slide;
		}
	}

	int id = -1;
	int sort_order = -1;
	int queue_id = -1;

	switch ( ret ){
		case SQLITE_ROW:
			/* read columns */
			id              =                     sqlite3_column_int (this->query_slide, 0);
			slide.filename  = strdup((const char*)sqlite3_column_text(this->query_slide, 1));
			sort_order      =                     sqlite3_column_int (this->query_slide, 2);
			queue_id        =                     sqlite3_column_int (this->query_slide, 3);
			slide.assembler = strdup((const char*)sqlite3_column_text(this->query_slide, 4));

			//Log::message(Log::Info, "slide: %s\n", slide.filename);
			//Log::message(Log::Debug, "\tid: %d\n", id);
			//Log::message(Log::Debug, "\tsort_order: %d\n", sort_order);
			//Log::message(Log::Debug, "\tqueue_id: %d\n", queue_id);

			/* only update id if it comes from a regular queue, i.e., not from intermediate queue. */
			if ( queue_id > 0 ){
				this->prev_slide_id = sort_order;
			} else {
				/* pop intermediate slides back to unsorted */
				//Log::message(Log::Debug, "popping intermediate slide\n", slide.filename, id, queue_id);
				pop_intermediate(this, id);
			}

			break;
		case SQLITE_MISUSE:
			//Log::message(Log::Info, "query_slide::sqlite3_step failed: SQLITE_MISUSE\n");
			//Log::message(Log::Debug, "\tqueue_id: %d\n", current_bin());
			//Log::message(Log::Debug, "\told_id: %d\n", _old_id);
			break;
		default:
			//Log::message(Log::Info, "query_slide::sqlite3_step failed: %d\n", ret);
			break;
	}

	sqlite3_reset(this->query_slide);

	return slide;
}

void EXPORT queue_reload(sqlite3_context_t* this){

}

void EXPORT queue_dump(sqlite3_context_t* this){

}

int EXPORT queue_set(sqlite3_context_t* this, unsigned int id){
	/* if we change queue we reset the position back to the start */
	if ( this->queue_id != id ){
		this->prev_slide_id = -1;
	}

	this->queue_id = id;

	/* query whenever to loop this queue */
	sqlite3_bind_int(this->query_looping, 1, id);
	int ret = sqlite3_step(this->query_looping);

	this->loop_queue = 1;

	switch ( ret ){
		case SQLITE_DONE:
			break;
		case SQLITE_ROW:
			this->loop_queue = sqlite3_column_int(this->query_looping, 0);
			break;
		case SQLITE_MISUSE:
			//Log::message(Log::Info, "query_loop::sqlite3_step failed: SQLITE_MISUSE\n");
			break;
		default:
			//Log::message(Log::Info, "query_loop::sqlite3_step failed: %d\n", ret);
			break;
	}

	sqlite3_reset(this->query_looping);
	return ret;
}