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
 */

#include "xauth_manager.h"

#include <utils/linked_list.h>
#include <threading/rwlock.h>

typedef struct private_xauth_manager_t private_xauth_manager_t;
typedef struct xauth_entry_t xauth_entry_t;

/**
 * XAuth constructor entry
 */
struct xauth_entry_t {

	/**
	 * XAuth method type, vendor specific if vendor is set
	 */
	xauth_type_t type;

	/**
	 * vendor ID, 0 for default XAuth methods
	 */
	u_int32_t vendor;

	/**
	 * Role of the method returned by the constructor, XAUTH_SERVER or XAUTH_PEER
	 */
	xauth_role_t role;

	/**
	 * constructor function to create instance
	 */
	xauth_constructor_t constructor;
};

/**
 * private data of xauth_manager
 */
struct private_xauth_manager_t {

	/**
	 * public functions
	 */
	xauth_manager_t public;

	/**
	 * list of eap_entry_t's
	 */
	linked_list_t *methods;

	/**
	 * rwlock to lock methods
	 */
	rwlock_t *lock;
};

METHOD(xauth_manager_t, add_method, void,
	private_xauth_manager_t *this, xauth_type_t type, u_int32_t vendor,
	xauth_role_t role, xauth_constructor_t constructor)
{
	xauth_entry_t *entry = malloc_thing(xauth_entry_t);

	entry->type = type;
	entry->vendor = vendor;
	entry->role = role;
	entry->constructor = constructor;

	this->lock->write_lock(this->lock);
	this->methods->insert_last(this->methods, entry);
	this->lock->unlock(this->lock);
}

METHOD(xauth_manager_t, remove_method, void,
	private_xauth_manager_t *this, xauth_constructor_t constructor)
{
	enumerator_t *enumerator;
	xauth_entry_t *entry;

	this->lock->write_lock(this->lock);
	enumerator = this->methods->create_enumerator(this->methods);
	while (enumerator->enumerate(enumerator, &entry))
	{
		if (constructor == entry->constructor)
		{
			this->methods->remove_at(this->methods, enumerator);
			free(entry);
		}
	}
	enumerator->destroy(enumerator);
	this->lock->unlock(this->lock);
}

METHOD(xauth_manager_t, create_instance, xauth_method_t*,
	private_xauth_manager_t *this, xauth_type_t type, u_int32_t vendor,
	xauth_role_t role, identification_t *server, identification_t *peer)
{
	enumerator_t *enumerator;
	xauth_entry_t *entry;
	xauth_method_t *method = NULL;

	this->lock->read_lock(this->lock);
	enumerator = this->methods->create_enumerator(this->methods);
	while (enumerator->enumerate(enumerator, &entry))
	{
		if (type == entry->type && vendor == entry->vendor &&
			role == entry->role)
		{
			method = entry->constructor(server, peer);
			if (method)
			{
				break;
			}
		}
	}
	enumerator->destroy(enumerator);
	this->lock->unlock(this->lock);
	return method;
}

METHOD(xauth_manager_t, destroy, void,
	private_xauth_manager_t *this)
{
	this->methods->destroy_function(this->methods, free);
	this->lock->destroy(this->lock);
	free(this);
}

/*
 * See header
 */
xauth_manager_t *xauth_manager_create()
{
	private_xauth_manager_t *this;

	INIT(this,
			.public = {
				.add_method = _add_method,
				.remove_method = _remove_method,
				.create_instance = _create_instance,
				.destroy = _destroy,
			},
			.methods = linked_list_create(),
			.lock = rwlock_create(RWLOCK_TYPE_DEFAULT),
	);

	return &this->public;
}
