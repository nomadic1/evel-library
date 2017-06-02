/**************************************************************************//**
 * @file
 * Implementation of EVEL functions relating to Event Headers - since
 * Heartbeats only contain the Event Header, the Heartbeat factory function is
 * here too.
 *
 * License
 * -------
 *
 * Copyright(c) <2016>, AT&T Intellectual Property.  All other rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:  This product includes
 *    software developed by the AT&T.
 * 4. Neither the name of AT&T nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AT&T INTELLECTUAL PROPERTY ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AT&T INTELLECTUAL PROPERTY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>

#include "evel.h"
#include "evel_internal.h"
#include "evel_throttle.h"
#include "metadata.h"

/**************************************************************************//**
 * Unique sequence number for events from this VNF.
 *****************************************************************************/
static int event_sequence = 1;

/**************************************************************************//**
 * Set the next event_sequence to use.
 *
 * @param sequence      The next sequence number to use.
 *****************************************************************************/
void evel_set_next_event_sequence(const int sequence)
{
  EVEL_ENTER();

  EVEL_INFO("Setting event sequence to %d, was %d ", sequence, event_sequence);
  event_sequence = sequence;

  EVEL_EXIT();
}

/**************************************************************************//**
 * Create a new heartbeat event.
 *
 * @note that the heartbeat is just a "naked" commonEventHeader!
 *
 * @returns pointer to the newly manufactured ::EVENT_HEADER.  If the event is
 *          not used it must be released using ::evel_free_event
 * @retval  NULL  Failed to create the event.
 *****************************************************************************/
EVENT_HEADER * evel_new_heartbeat()
{
  EVENT_HEADER * heartbeat = NULL;
  EVEL_ENTER();

  /***************************************************************************/
  /* Allocate the header.                                                    */
  /***************************************************************************/
  heartbeat = malloc(sizeof(EVENT_HEADER));
  if (heartbeat == NULL)
  {
    log_error_state("Out of memory");
    goto exit_label;
  }
  memset(heartbeat, 0, sizeof(EVENT_HEADER));

  /***************************************************************************/
  /* Initialize the header.  Get a new event sequence number.  Note that if  */
  /* any memory allocation fails in here we will fail gracefully because     */
  /* everything downstream can cope with NULLs.                              */
  /***************************************************************************/
  evel_init_header(heartbeat,"Heartbeat");
  evel_force_option_string(&heartbeat->event_type, "Autonomous heartbeat");

exit_label:
  EVEL_EXIT();
  return heartbeat;
}

/**************************************************************************//**
 * Initialize a newly created event header.
 *
 * @param header  Pointer to the header being initialized.
 *****************************************************************************/
void evel_init_header(EVENT_HEADER * const header,const char *const eventname)
{
  char scratchpad[EVEL_MAX_STRING_LEN + 1] = {0};
  struct timeval tv;

  EVEL_ENTER();

  assert(header != NULL);

  gettimeofday(&tv, NULL);

  /***************************************************************************/
  /* Initialize the header.  Get a new event sequence number.  Note that if  */
  /* any memory allocation fails in here we will fail gracefully because     */
  /* everything downstream can cope with NULLs.                              */
  /***************************************************************************/
  header->event_domain = EVEL_DOMAIN_HEARTBEAT;
  snprintf(scratchpad, EVEL_MAX_STRING_LEN, "%d", event_sequence);
  header->event_id = strdup(scratchpad);
  if( eventname == NULL )
     header->event_name = strdup(functional_role);
  else
     header->event_name = strdup(eventname);
  header->last_epoch_microsec = tv.tv_usec + 1000000 * tv.tv_sec;
  header->priority = EVEL_PRIORITY_NORMAL;
  header->reporting_entity_name = strdup(openstack_vm_name());
  header->source_name = strdup(openstack_vm_name());
  header->sequence = event_sequence;
  header->start_epoch_microsec = header->last_epoch_microsec;
  header->major_version = EVEL_HEADER_MAJOR_VERSION;
  header->minor_version = EVEL_HEADER_MINOR_VERSION;
  event_sequence++;

  /***************************************************************************/
  /* Optional parameters.                                                    */
  /***************************************************************************/
  evel_init_option_string(&header->event_type);
  evel_init_option_string(&header->nfcnaming_code);
  evel_init_option_string(&header->nfnaming_code);
  evel_force_option_string(&header->reporting_entity_id, openstack_vm_uuid());
  evel_force_option_string(&header->source_id, openstack_vm_uuid());
  evel_init_option_intheader(&header->internal_field);

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the Event Type property of the event header.
 *
 * @note  The property is treated as immutable: it is only valid to call
 *        the setter once.  However, we don't assert if the caller tries to
 *        overwrite, just ignoring the update instead.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param type          The Event Type to be set. ASCIIZ string. The caller
 *                      does not need to preserve the value once the function
 *                      returns.
 *****************************************************************************/
void evel_header_type_set(EVENT_HEADER * const header,
                          const char * const type)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(header != NULL);
  assert(type != NULL);

  evel_set_option_string(&header->event_type, type, "Event Type");

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the Start Epoch property of the event header.
 *
 * @note The Start Epoch defaults to the time of event creation.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param start_epoch_microsec
 *                      The start epoch to set, in microseconds.
 *****************************************************************************/
void evel_start_epoch_set(EVENT_HEADER * const header,
                          const unsigned long long start_epoch_microsec)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions and assign the new value.                           */
  /***************************************************************************/
  assert(header != NULL);
  header->start_epoch_microsec = start_epoch_microsec;

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the Last Epoch property of the event header.
 *
 * @note The Last Epoch defaults to the time of event creation.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param last_epoch_microsec
 *                      The last epoch to set, in microseconds.
 *****************************************************************************/
void evel_last_epoch_set(EVENT_HEADER * const header,
                         const unsigned long long last_epoch_microsec)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions and assign the new value.                           */
  /***************************************************************************/
  assert(header != NULL);
  header->last_epoch_microsec = last_epoch_microsec;

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the NFC Naming code property of the event header.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param nfcnamingcode String
 *****************************************************************************/
void evel_nfcnamingcode_set(EVENT_HEADER * const header,
                         const char * const nfcnam)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions and assign the new value.                           */
  /***************************************************************************/
  assert(header != NULL);
  assert(nfcnam != NULL);
  evel_set_option_string(&header->nfcnaming_code, nfcnam, "NFC Naming Code");

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the NF Naming code property of the event header.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param nfnamingcode String
 *****************************************************************************/
void evel_nfnamingcode_set(EVENT_HEADER * const header,
                         const char * const nfnam)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions and assign the new value.                           */
  /***************************************************************************/
  assert(header != NULL);
  assert(nfnam != NULL);
  evel_set_option_string(&header->nfnaming_code, nfnam, "NF Naming Code");

  EVEL_EXIT();
}


/**************************************************************************//**
 * Set the Reporting Entity Name property of the event header.
 *
 * @note The Reporting Entity Name defaults to the OpenStack VM Name.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param entity_name   The entity name to set.
 *****************************************************************************/
void evel_reporting_entity_name_set(EVENT_HEADER * const header,
                                    const char * const entity_name)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions and assign the new value.                           */
  /***************************************************************************/
  assert(header != NULL);
  assert(entity_name != NULL);
  assert(header->reporting_entity_name != NULL);

  /***************************************************************************/
  /* Free the previously allocated memory and replace it with a copy of the  */
  /* provided one.                                                           */
  /***************************************************************************/
  free(header->reporting_entity_name);
  header->reporting_entity_name = strdup(entity_name);

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the Reporting Entity Id property of the event header.
 *
 * @note The Reporting Entity Id defaults to the OpenStack VM UUID.
 *
 * @param header        Pointer to the ::EVENT_HEADER.
 * @param entity_id     The entity id to set.
 *****************************************************************************/
void evel_reporting_entity_id_set(EVENT_HEADER * const header,
                                  const char * const entity_id)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions and assign the new value.                           */
  /***************************************************************************/
  assert(header != NULL);
  assert(entity_id != NULL);

  /***************************************************************************/
  /* Free the previously allocated memory and replace it with a copy of the  */
  /* provided one.  Note that evel_force_option_string strdups entity_id.    */
  /***************************************************************************/
  evel_free_option_string(&header->reporting_entity_id);
  evel_force_option_string(&header->reporting_entity_id, entity_id);

  EVEL_EXIT();
}

/**************************************************************************//**
 * Encode the event as a JSON event object according to AT&T's schema.
 *
 * @param jbuf          Pointer to the ::EVEL_JSON_BUFFER to encode into.
 * @param event         Pointer to the ::EVENT_HEADER to encode.
 *****************************************************************************/
void evel_json_encode_header(EVEL_JSON_BUFFER * jbuf,
                             EVENT_HEADER * event)
{
  char * domain;
  char * priority;

  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(jbuf != NULL);
  assert(jbuf->json != NULL);
  assert(jbuf->max_size > 0);
  assert(event != NULL);

  domain = evel_event_domain(event->event_domain);
  priority = evel_event_priority(event->priority);
  evel_json_open_named_object(jbuf, "commonEventHeader");

  /***************************************************************************/
  /* Mandatory fields.                                                       */
  /***************************************************************************/
  evel_enc_kv_string(jbuf, "domain", domain);
  evel_enc_kv_string(jbuf, "eventId", event->event_id);
  evel_enc_kv_string(jbuf, "eventName", event->event_name);
  evel_enc_kv_ull(jbuf, "lastEpochMicrosec", event->last_epoch_microsec);
  evel_enc_kv_string(jbuf, "priority", priority);
  evel_enc_kv_string(
    jbuf, "reportingEntityName", event->reporting_entity_name);
  evel_enc_kv_int(jbuf, "sequence", event->sequence);
  evel_enc_kv_string(jbuf, "sourceName", event->source_name);
  evel_enc_kv_ull(jbuf, "startEpochMicrosec", event->start_epoch_microsec);
  evel_enc_version(
    jbuf, "version", event->major_version, event->minor_version);

  /***************************************************************************/
  /* Optional fields.                                                        */
  /***************************************************************************/
  evel_enc_kv_opt_string(jbuf, "eventType", &event->event_type);
  evel_enc_kv_opt_string(
    jbuf, "reportingEntityId", &event->reporting_entity_id);
  evel_enc_kv_opt_string(jbuf, "sourceId", &event->source_id);
  evel_enc_kv_opt_string(jbuf, "nfcNamingCode", &event->nfcnaming_code);
  evel_enc_kv_opt_string(jbuf, "nfNamingCode", &event->nfnaming_code);

  evel_json_close_object(jbuf);

  EVEL_EXIT();
}

/**************************************************************************//**
 * Free an event header.
 *
 * Free off the event header supplied.  Will free all the contained allocated
 * memory.
 *
 * @note It does not free the header itself, since that may be part of a
 * larger structure.
 *****************************************************************************/
void evel_free_header(EVENT_HEADER * const event)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.  As an internal API we don't allow freeing NULL    */
  /* events as we do on the public API.                                      */
  /***************************************************************************/
  assert(event != NULL);

  /***************************************************************************/
  /* Free all internal strings.                                              */
  /***************************************************************************/
  free(event->event_id);
  evel_free_option_string(&event->event_type);
  free(event->event_name);
  evel_free_option_string(&event->reporting_entity_id);
  free(event->reporting_entity_name);
  evel_free_option_string(&event->source_id);
  evel_free_option_string(&event->nfcnaming_code);
  evel_free_option_string(&event->nfnaming_code);
  evel_free_option_intheader(&event->internal_field);
  free(event->source_name);

  EVEL_EXIT();
}

/**************************************************************************//**
 * Encode the event as a JSON event object according to AT&T's schema.
 *
 * @param json      Pointer to where to store the JSON encoded data.
 * @param max_size  Size of storage available in json_body.
 * @param event     Pointer to the ::EVENT_HEADER to encode.
 * @returns Number of bytes actually written.
 *****************************************************************************/
int evel_json_encode_event(char * json,
                           int max_size,
                           EVENT_HEADER * event)
{
  EVEL_JSON_BUFFER json_buffer;
  EVEL_JSON_BUFFER * jbuf = &json_buffer;
  EVEL_THROTTLE_SPEC * throttle_spec;

  EVEL_ENTER();

  /***************************************************************************/
  /* Get the latest throttle specification for the domain.                   */
  /***************************************************************************/
  throttle_spec = evel_get_throttle_spec(event->event_domain);

  /***************************************************************************/
  /* Initialize the JSON_BUFFER and open the top-level objects.              */
  /***************************************************************************/
  evel_json_buffer_init(jbuf, json, max_size, throttle_spec);
  evel_json_open_object(jbuf);
  evel_json_open_named_object(jbuf, "event");

  switch (event->event_domain)
  {
    case EVEL_DOMAIN_HEARTBEAT:
      evel_json_encode_header(jbuf, event);
      break;

    case EVEL_DOMAIN_FAULT:
      evel_json_encode_fault(jbuf, (EVENT_FAULT *)event);
      break;

    case EVEL_DOMAIN_MEASUREMENT:
      evel_json_encode_measurement(jbuf, (EVENT_MEASUREMENT *)event);
      break;

    case EVEL_DOMAIN_MOBILE_FLOW:
      evel_json_encode_mobile_flow(jbuf, (EVENT_MOBILE_FLOW *)event);
      break;

    case EVEL_DOMAIN_REPORT:
      evel_json_encode_report(jbuf, (EVENT_REPORT *)event);
      break;

    case EVEL_DOMAIN_HEARTBEAT_FIELD:
      evel_json_encode_hrtbt_field(jbuf, (EVENT_HEARTBEAT_FIELD *)event);
      break;

    case EVEL_DOMAIN_SIPSIGNALING:
      evel_json_encode_signaling(jbuf, (EVENT_SIGNALING *)event);
      break;

    case EVEL_DOMAIN_STATE_CHANGE:
      evel_json_encode_state_change(jbuf, (EVENT_STATE_CHANGE *)event);
      break;

    case EVEL_DOMAIN_SYSLOG:
      evel_json_encode_syslog(jbuf, (EVENT_SYSLOG *)event);
      break;

    case EVEL_DOMAIN_OTHER:
      evel_json_encode_other(jbuf, (EVENT_OTHER *)event);
      break;

    case EVEL_DOMAIN_VOICE_QUALITY:
      evel_json_encode_other(jbuf, (EVENT_VOICE_QUALITY *)event);
      break;

    case EVEL_DOMAIN_INTERNAL:
    default:
      EVEL_ERROR("Unexpected domain %d", event->event_domain);
      assert(0);
  }

  evel_json_close_object(jbuf);
  evel_json_close_object(jbuf);

  /***************************************************************************/
  /* Sanity check.                                                           */
  /***************************************************************************/
  assert(jbuf->depth == 0);

  EVEL_EXIT();

  return jbuf->offset;
}


/**************************************************************************//**
 * Initialize an event instance id.
 *
 * @param vfield        Pointer to the event vnfname field being initialized.
 * @param vendor_id     The vendor id to encode in the event instance id.
 * @param event_id      The event id to encode in the event instance id.
 *****************************************************************************/
void evel_init_vendor_field(VENDOR_VNFNAME_FIELD * const vfield,
                                 const char * const vendor_name)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(vfield != NULL);
  assert(vendor_name != NULL);

  /***************************************************************************/
  /* Store the mandatory parts.                                              */
  /***************************************************************************/
  vfield->vendorname = strdup(vendor_name);
  evel_init_option_string(&vfield->vfmodule);
  evel_init_option_string(&vfield->vnfname);

  /***************************************************************************/
  /* Initialize the optional parts.                                          */
  /***************************************************************************/

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the Vendor module property of the Vendor.
 *
 * @note  The property is treated as immutable: it is only valid to call
 *        the setter once.  However, we don't assert if the caller tries to
 *        overwrite, just ignoring the update instead.
 *
 * @param vfield        Pointer to the Vendor field.
 * @param module_name   The module name to be set. ASCIIZ string. The caller
 *                      does not need to preserve the value once the function
 *                      returns.
 *****************************************************************************/
void evel_vendor_field_module_set(VENDOR_VNFNAME_FIELD * const vfield,
                                    const char * const module_name)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(vfield != NULL);
  assert(module_name != NULL);

  evel_set_option_string(&vfield->vfmodule, module_name, "Module name set");

  EVEL_EXIT();
}

/**************************************************************************//**
 * Set the Vendor module property of the Vendor.
 *
 * @note  The property is treated as immutable: it is only valid to call
 *        the setter once.  However, we don't assert if the caller tries to
 *        overwrite, just ignoring the update instead.
 *
 * @param vfield        Pointer to the Vendor field.
 * @param module_name   The module name to be set. ASCIIZ string. The caller
 *                      does not need to preserve the value once the function
 *                      returns.
 *****************************************************************************/
void evel_vendor_field_vnfname_set(VENDOR_VNFNAME_FIELD * const vfield,
                                    const char * const vnfname)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(vfield != NULL);
  assert(vnfname != NULL);

  evel_set_option_string(&vfield->vnfname, vnfname, "Virtual Network Function name set");

  EVEL_EXIT();
}

/**************************************************************************//**
 * Free an event instance id.
 *
 * @param vfield   Pointer to the event vnfname_field being freed.
 *****************************************************************************/
void evel_free_event_vendor_field(VENDOR_VNFNAME_FIELD * const vfield)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(vfield->vendorname != NULL);

  /***************************************************************************/
  /* Free everything.                                                        */
  /***************************************************************************/
  evel_free_option_string(&vfield->vfmodule);
  evel_free_option_string(&vfield->vnfname);
  free(vfield->vendorname);

  EVEL_EXIT();
}

/**************************************************************************//**
 * Encode the instance id as a JSON object according to AT&T's schema.
 *
 * @param jbuf          Pointer to the ::EVEL_JSON_BUFFER to encode into.
 * @param vfield        Pointer to the ::VENDOR_VNFNAME_FIELD to encode.
 *****************************************************************************/
void evel_json_encode_vendor_field(EVEL_JSON_BUFFER * jbuf,
                                  VENDOR_VNFNAME_FIELD * vfield)
{
  EVEL_ENTER();

  /***************************************************************************/
  /* Check preconditions.                                                    */
  /***************************************************************************/
  assert(jbuf != NULL);
  assert(jbuf->json != NULL);
  assert(jbuf->max_size > 0);
  assert(vfield != NULL);
  assert(vfield->vendorname != NULL);

  evel_json_open_named_object(jbuf, "vendorVnfNamedFields");

  /***************************************************************************/
  /* Mandatory fields.                                                       */
  /***************************************************************************/
  evel_enc_kv_string(jbuf, "vendorName", vfield->vendorname);
  evel_enc_kv_opt_string(jbuf, "vfModuleName", &vfield->vfmodule);
  evel_enc_kv_opt_string(jbuf, "vnfName", &vfield->vnfname);

  /***************************************************************************/
  /* Optional fields.                                                        */
  /***************************************************************************/

  evel_json_close_object(jbuf);

  EVEL_EXIT();
}
