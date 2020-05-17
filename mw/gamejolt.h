/************************************************************************//**
 * \file
 *
 * \brief GameJolt Game API implementation for MegaWiFi.
 *
 * \defgroup gamejolt gamejolt
 * \{
 *
 * \brief GameJolt Game API implementation for MegaWiFi.
 *
 * Implementation of the version 1.2 of the GameJolt Game API, supporting
 * scoreboards, trophies, friends, etc.
 *
 * \author Jesus Alonso (doragasu)
 * \date 2020
 *
 * \note The module uses X Macros, making Doxygen documentation of some
 * elements a bit complicated. Sorry about that.
 ****************************************************************************/

#ifndef _GAMEJOLT_H_
#define _GAMEJOLT_H_

#include <stdbool.h>
#include <stdint.h>

/// Difficulty to achieve the trophy
enum gj_trophy_difficulty {
	GJ_TROPHY_TYPE_BRONZE = 0,	///< Bronze trophy (easiest)
	GJ_TROPHY_TYPE_SILVER,		///< Silver trophy (medium)
	GJ_TROPHY_TYPE_GOLD,		///< Gold trophy (hard)
	GJ_TROPHY_TYPE_PLATINUM,	///< Platinum trophy (hardest)
	GJ_TROPHY_TYPE_UNKNOWN		///< Unknown, just for errors
};

/// Reply fields to a trophy fetch request. To be used with X Macros
#define GJ_TROPHY_RESPONSE_TABLE(X_MACRO) \
	X_MACRO(id,          string,            char*) \
	X_MACRO(title,       string,            char*) \
	X_MACRO(difficulty,  trophy_difficulty, enum gj_trophy_difficulty) \
	X_MACRO(description, string,            char*) \
	X_MACRO(image_url,   string,            char*) \
	X_MACRO(achieved,    string,            char*)

/// Expands a response table as a structure with its fields
#define X_AS_STRUCT(field, decoder, type) \
	type field;

/// Holds the data of a single trophy.
struct gj_trophy {
	GJ_TROPHY_RESPONSE_TABLE(X_AS_STRUCT);
	bool secret;	///< If true, trophy is secret
};

/************************************************************************//**
 * \brief Initialize the GameJolt API.
 *
 * This function sets the API endpoint, game credentials and user credentials.
 * Call this function before using any other function in the module.
 *
 * \param[in] endpoint    Endpoint for the Game API. Most likely you want to
 *            use "https://api.gamejolt.com/api/game/v1_2/" here.
 * \param[in] game_id     Game identifier. E.g. "123456"
 * \param[in] private_key Game private key. Keep it safe!
 * \param[in] username    Username of the player.
 * \param[in] user_token  Token corresponding to username.
 * \param[in] reply_buf   Pre-allocated buffer to use for data reception.
 * \param[in] buf_len     Length of reply_buf buffer.
 *
 * \return false on success, true on error.
 *
 * \note reply_buf length determines the maximum length of the reply to a
 * command. If buffer length is small, API will not be able to receive
 * long responses such as trophy lists or long friend lists.
 ****************************************************************************/
bool gj_init(const char *endpoint, const char *game_id, const char *private_key,
		const char *username, const char *user_token, char *reply_buf,
		uint16_t buf_len);

/************************************************************************//**
 * \brief Fetch player trophies.
 *
 * \param[in] achieved    If true, only achieved trophies are get.
 * \param[in] trophy_id   If not NULL, a single trophy with specified id
 *            is retrieved.
 * \param[in] tout_frames Number of frames to wait for reply before timing out.
 *
 * \return Raw trophy data on success, NULL on failure. Use gh_trophy_get_next()
 * to decode the raw information.
 ****************************************************************************/
char *gj_trophies_fetch(bool achieved, const char *trophy_id,
		uint16_t tout_frames);

/************************************************************************//**
 * \brief Decode the trophy raw data for the next entry.
 *
 * On first call, set pos to the value returned by gj_trophies_fetch(). On
 * successive calls, set pos to the last non-NULL returned value of this
 * function.
 *
 * \param[inout] pos    Position of the trophy to extract. Note that input
 *               raw data is modified to add null terminations for fields
 * \param[out]   trophy Decoded trophy data.
 *
 * \return Position of the next trophy to decode (to be used on next call
 * to this function), or NULL if the curren trophy could not be decoded.
 ****************************************************************************/
char *gj_trophy_get_next(char *pos, struct gj_trophy *trophy);

/************************************************************************//**
 * \brief Mark a trophy as achieved.
 *
 * \param[in] trophy_id   Identifier of the trophy to mark as achieved.
 * \param[in] tout_frames Number of frames to wait for reply before timing out.
 *
 * \return true if error, false on success.
 ****************************************************************************/
bool gj_trophy_add_achieved(const char *trophy_id, uint16_t tout_frames);

/************************************************************************//**
 * \brief Mark a trophy as not achieved.
 *
 * \param[in] trophy_id   Identifier of the trophy to mark as not achieved.
 * \param[in] tout_frames Number of frames to wait for reply before timing out.
 *
 * \return true if error, false on success.
 ****************************************************************************/
bool gj_trophy_remove_achieved(const char *trophy_id, uint16_t tout_frames);

/************************************************************************//**
 * \brief Get the string corresponding to a trophy difficulty.
 *
 * \param[in] difficulty Difficulty value to be translated to a string.
 *
 * \return The string corresponding to the specified difficulty. If the input
 * value of difficulty is out of range, "Unknown" string will be returned.
 ****************************************************************************/
const char *gj_trophy_difficulty_str(enum gj_trophy_difficulty difficulty);

/************************************************************************//**
 * \brief Generic GameJolt Game API request.
 *
 * Usually you do not need to use this function directly. Use more specific
 * API calls that do the hard work of filling parameters and decoding the
 * response.
 *
 * \param[in]  path         Array of paths for the request.
 * \param[in]  num_paths    Number of elements in path array.
 * \param[in]  key          Array of keys for key/value parameters.
 * \param[in]  value        Array of values for key/value parameters.
 * \param[in]  num_kv_pairs Number of elements in key and value arrays.
 * \param[in]  tout_frames  Frames to wait for reply before timing out.
 * \param[out] out_len      Length of the received reply to request.
 *
 * \return The string corresponding to the specified difficulty. If the input
 * value of difficulty is out of range, "Unknown" string will be returned.
 ****************************************************************************/
char *gj_request(const char **path, uint8_t num_paths, const char **key,
		const char **value, uint8_t num_kv_pairs, uint16_t tout_frames,
		uint32_t *out_len);

#endif /*_GAMEJOLT_H_*/

/** \} */

