/* This file was generated by upb_generator from the input file:
 *
 *     envoy/extensions/filters/http/gcp_authn/v3/gcp_authn.proto
 *
 * Do not edit -- your changes will be discarded when the file is
 * regenerated. */

#ifndef ENVOY_EXTENSIONS_FILTERS_HTTP_GCP_AUTHN_V3_GCP_AUTHN_PROTO_UPBDEFS_H_
#define ENVOY_EXTENSIONS_FILTERS_HTTP_GCP_AUTHN_V3_GCP_AUTHN_PROTO_UPBDEFS_H_

#include "upb/reflection/def.h"
#include "upb/reflection/internal/def_pool.h"

#include "upb/port/def.inc" // Must be last.
#ifdef __cplusplus
extern "C" {
#endif

extern _upb_DefPool_Init envoy_extensions_filters_http_gcp_authn_v3_gcp_authn_proto_upbdefinit;

UPB_INLINE const upb_MessageDef *envoy_extensions_filters_http_gcp_authn_v3_GcpAuthnFilterConfig_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_filters_http_gcp_authn_v3_gcp_authn_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.filters.http.gcp_authn.v3.GcpAuthnFilterConfig");
}

UPB_INLINE const upb_MessageDef *envoy_extensions_filters_http_gcp_authn_v3_Audience_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_filters_http_gcp_authn_v3_gcp_authn_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.filters.http.gcp_authn.v3.Audience");
}

UPB_INLINE const upb_MessageDef *envoy_extensions_filters_http_gcp_authn_v3_TokenCacheConfig_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_filters_http_gcp_authn_v3_gcp_authn_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.filters.http.gcp_authn.v3.TokenCacheConfig");
}

UPB_INLINE const upb_MessageDef *envoy_extensions_filters_http_gcp_authn_v3_TokenHeader_getmsgdef(upb_DefPool *s) {
  _upb_DefPool_LoadDefInit(s, &envoy_extensions_filters_http_gcp_authn_v3_gcp_authn_proto_upbdefinit);
  return upb_DefPool_FindMessageByName(s, "envoy.extensions.filters.http.gcp_authn.v3.TokenHeader");
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#include "upb/port/undef.inc"

#endif  /* ENVOY_EXTENSIONS_FILTERS_HTTP_GCP_AUTHN_V3_GCP_AUTHN_PROTO_UPBDEFS_H_ */
