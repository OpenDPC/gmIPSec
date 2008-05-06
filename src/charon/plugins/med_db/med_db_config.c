/*
 * Copyright (C) 2008 Martin Willi
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * $Id$
 */

#include <string.h>

#include "med_db_config.h"

#include <daemon.h>

typedef struct private_med_db_config_t private_med_db_config_t;

/**
 * Private data of an med_db_config_t object
 */
struct private_med_db_config_t {

	/**
	 * Public part
	 */
	med_db_config_t public;
	
	/**
	 * database connection
	 */
	database_t *db;
	
	/**
	 * rekey time
	 */
	int rekey;
	
	/**
	 * dpd delay
	 */
	int dpd;
	
	/**
	 * default ike config
	 */
	ike_cfg_t *ike;
};

/**
 * implements backend_t.get_peer_cfg_by_name.
 */
static peer_cfg_t *get_peer_cfg_by_name(private_med_db_config_t *this, char *name)
{
	return NULL;
}

/**
 * Implementation of backend_t.create_ike_cfg_enumerator.
 */
static enumerator_t* create_ike_cfg_enumerator(private_med_db_config_t *this,
											   host_t *me, host_t *other)
{
	return enumerator_create_single(this->ike, NULL);
}

/**
 * Implementation of backend_t.create_peer_cfg_enumerator.
 */
static enumerator_t* create_peer_cfg_enumerator(private_med_db_config_t *this,
												identification_t *me,
												identification_t *other)
{
	enumerator_t *e;
	
	if (!me || !other || other->get_type(other) != ID_KEY_ID)
	{
		return NULL;
	}
	e = this->db->query(this->db,
			"SELECT CONCAT(Peer.Alias, CONCAT('@', User.Login)) FROM "
			"Peer JOIN User ON Peer.IdUser = User.IdUser "
			"WHERE Peer.KeyID = ?", DB_BLOB, other->get_encoding(other),
			DB_TEXT);
	if (e)
	{
		peer_cfg_t *peer_cfg;
		char *name;
		
		if (e->enumerate(e, &name))
		{
			peer_cfg = peer_cfg_create(
				name, 2, this->ike->get_ref(this->ike),
				me->clone(me), other->clone(other),
				CERT_NEVER_SEND, UNIQUE_REPLACE, AUTH_RSA,
				0, 0, 							/* EAP method, vendor */
				1, this->rekey*60, 0,  			/* keytries, rekey, reauth */
				this->rekey*5, this->rekey*3, 	/* jitter, overtime */
				TRUE, this->dpd, 				/* mobike, dpddelay */
				NULL, NULL, 					/* vip, pool */
				TRUE, NULL, NULL); 				/* mediation, med by, peer id */
			e->destroy(e);
			return enumerator_create_single(peer_cfg, (void*)peer_cfg->destroy);
		}
		e->destroy(e);
	}
	return NULL;
}

/**
 * Implementation of med_db_config_t.destroy.
 */
static void destroy(private_med_db_config_t *this)
{
	this->ike->destroy(this->ike);
	free(this);
}

/**
 * Described in header.
 */
med_db_config_t *med_db_config_create(database_t *db)
{
	private_med_db_config_t *this = malloc_thing(private_med_db_config_t);

	this->public.backend.create_peer_cfg_enumerator = (enumerator_t*(*)(backend_t*, identification_t *me, identification_t *other))create_peer_cfg_enumerator;
	this->public.backend.create_ike_cfg_enumerator = (enumerator_t*(*)(backend_t*, host_t *me, host_t *other))create_ike_cfg_enumerator;
	this->public.backend.get_peer_cfg_by_name = (peer_cfg_t* (*)(backend_t*,char*))get_peer_cfg_by_name;
	this->public.destroy = (void(*)(med_db_config_t*))destroy;
	
	this->db = db;
	this->rekey = lib->settings->get_int(lib->settings,
										 "medmanager.rekey", 20) * 60;
	this->dpd = lib->settings->get_int(lib->settings, "medmanager.dpd", 300);
	this->ike = ike_cfg_create(FALSE, FALSE, host_create_any(AF_INET),
							   host_create_any(AF_INET));
	this->ike->add_proposal(this->ike, proposal_create_default(PROTO_IKE));
	
	return &this->public;
}

