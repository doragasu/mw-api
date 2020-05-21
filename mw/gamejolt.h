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

/// Reply fields to a trophy fetch request.
#define GJ_TROPHY_RESPONSE_TABLE(X_MACRO) \
	X_MACRO(id,          string,            char*) \
	X_MACRO(title,       string,            char*) \
	X_MACRO(difficulty,  trophy_difficulty, enum gj_trophy_difficulty) \
	X_MACRO(description, string,            char*) \
	X_MACRO(image_url,   string,            char*) \
	X_MACRO(achieved,    string,            char*)

/// Reply fields to a time fetch request.
#define GJ_TIME_RESPONSE_TABLE(X_MACRO) \
	X_MACRO(timestamp, string, char*) \
	X_MACRO(timezone,  string, char*) \
	X_MACRO(day,       string, char*) \
	X_MACRO(hour,      string, char*) \
	X_MACRO(minute,    string, char*) \
	X_MACRO(second,    string, char*)

/// Reply field to a scores fetch request.
#define GJ_SCORE_RESPONSE_TABLE(X_MACRO) \
	X_MACRO(score,            string, char*) \
	X_MACRO(sort,             string, char*) \
	X_MACRO(extra_data,       string, char*) \
	X_MACRO(user,             string, char*) \
	X_MACRO(user_id,          string, char*) \
	X_MACRO(guest,            string, char*) \
	X_MACRO(stored,           string, char*) \
	X_MACRO(stored_timestamp, string, char*)

#define GJ_SCORE_TABLE_RESPONSE_TABLE(X_MACRO) \
	X_MACRO(id,          string, char*) \
	X_MACRO(name,        string, char*) \
	X_MACRO(description, string, char*) \
	X_MACRO(primary,     bool_num, bool)

#define GJ_SCORE_GETRANK_RESPONSE_TABLE(X_MACRO) \
	X_MACRO(message, string, char*) \
	X_MACRO(rank,    string, char*)

/// Expands a response table as a structure with its fields
#define X_AS_STRUCT(field, decoder, type) \
	type field;

/// Holds the data of a single trophy.
struct gj_trophy {
	GJ_TROPHY_RESPONSE_TABLE(X_AS_STRUCT);
	bool secret;	///< If true, trophy is secret
};

/// Holds the date/time from server
struct gj_time {
	GJ_TIME_RESPONSE_TABLE(X_AS_STRUCT);
};

/// Holds data of a single score.
struct gj_score {
	GJ_SCORE_RESPONSE_TABLE(X_AS_STRUCT);
};

/// Holds data of a single score table.
struct gj_score_table {
	GJ_SCORE_TABLE_RESPONSE_TABLE(X_AS_STRUCT);
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
 * \param[in] tout_frames Number of frames to wait for requests before
 *                        timing out.
 *
 * \return false on success, true on error.
 *
 * \note reply_buf length determines the maximum length of the reply to a
 * command. If buffer length is small, API will not be able to receive
 * long responses such as trophy lists or long friend lists.
 ****************************************************************************/
bool gj_init(const char *endpoint, const char *game_id, const char *private_key,
		const char *username, const char *user_token, char *reply_buf,
		uint16_t buf_len, uint16_t tout_frames);

/************************************************************************//**
 * \brief Fetch player trophies.
 *
 * \param[in] achieved    If true, only achieved trophies are get.
 * \param[in] trophy_id   If not NULL, a single trophy with specified id
 *            is retrieved.
 *
 * \return Raw trophy data on success, NULL on failure. Use gh_trophy_get_next()
 * to decode the raw information.
 ****************************************************************************/
char *gj_trophies_fetch(bool achieved, const char *trophy_id);

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
 * to this function), or NULL if the current trophy could not be decoded.
 ****************************************************************************/
char *gj_trophy_get_next(char *pos, struct gj_trophy *trophy);

/************************************************************************//**
 * \brief Mark a trophy as achieved.
 *
 * \param[in] trophy_id   Identifier of the trophy to mark as achieved.
 *
 * \return true if error, false on success.
 ****************************************************************************/
bool gj_trophy_add_achieved(const char *trophy_id);

/************************************************************************//**
 * \brief Mark a trophy as not achieved.
 *
 * \param[in] trophy_id   Identifier of the trophy to mark as not achieved.
 *
 * \return true if error, false on success.
 ****************************************************************************/
bool gj_trophy_remove_achieved(const char *trophy_id);

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
 * \brief Get the date and time from server.
 *
 * \param[out] time        Date and time from the server.
 *
 * \return true if error, false on success.
 ****************************************************************************/
bool gj_time(struct gj_time *time);

/************************************************************************//**
 * \brief Fetch scores data.
 *
 * \param[in] limit       Number of scores to return (defaults to 10).
 * \param[in] table_id    Table id, or NULL for the main game table.
 * \param[in] guest       Name of the guest player, or NULL for user player.
 * \param[in] better_than Get only scores better than this sort value.
 * \param[in] worse_than  Get only scores worse than this sort value.
 *
 * \return Raw scores data on success, or NULL on failure.
 * Use gj_scores_get_next() to decode the raw score data.
 * \note All parameters are optional, use NULL if you do not want to set
 * them.
 ****************************************************************************/
char *gj_scores_fetch(const char *limit, const char *table_id,
		const char *guest, const char *better_than,
		const char *worse_than);

/************************************************************************//**
 * \brief Decode the score raw data for the next entry.
 *
 * On first call, set pos to the value returned by gj_scores_fetch(). On
 * successive calls, set pos to the last non-NULL returned value of this
 * function.
 *
 * \param[inout] pos   Position of the trophy to extract. Note that input
 *               raw data is modified to add null terminations for fields
 * \param[out]   score Decoded trophy data.
 *
 * \return Position of the next score to decode (to be used on next call
 * to this function), or NULL if the current score could not be decoded.
 ****************************************************************************/
char *gj_score_get_next(char *pos, struct gj_score *score);

/************************************************************************//**
 * \brief Fetch score tables.
 *
 * \return Raw score tables data on success, or NULL on failure.
 * Use gj_score_table_get_next() to decode the raw score table data.
 ****************************************************************************/
char *gj_scores_tables(void);

/************************************************************************//**
 * \brief Decode the score tables raw data for the next entry.
 *
 * On first call, set pos to the value returned by gj_scores_tables_fetch().
 * On successive calls, set pos to the last non-NULL returned value by this
 * function.
 *
 * \param[inout] pos         Position of the score table to extract. Note that
 *               input raw data is modified to add null terminations for fields
 * \param[out]   score_table Decoded score table data.
 *
 * \return Position of the next score to decode (to be used on next call
 * to this function), or NULL if the current score could not be decoded.
 ****************************************************************************/
char *gj_score_table_get_next(char *pos, struct gj_score_table *score_table);

/************************************************************************//**
 * \brief Get ranking corresponding to a sort parameter.
 *
 * \param[in] sort     Score sort value to get ranking position from table.
 * \param[in] table_id Score table id, or NULL for the main table.
 *
 * \return Ranking corresponding to sort parameter, or NULL on error.
 ****************************************************************************/
char *gj_scores_get_rank(const char *sort, const char *table_id);

/************************************************************************//**
 * \brief Add a score to a scoreboard.
 *
 * \param[in] score      Score in textual format (e.g. "500 torreznos")
 * \param[in] sort       Number used to sort the score (e.g. "500")
 * \param[in] table_id   Table id, or NULL for the main game table.
 * \param[in] guest      Name of the guest player, or NULL for user player.
 * \param[in] extra_data Extra data to save with score, NULL for none.
 *
 * \return true if error, false on success.
 * \note guest parameter is not yet supported.
 ****************************************************************************/
bool gj_scores_add(const char *score, const char *sort, const char *table_id,
		const char *guest, const char *extra_data);

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
 * \param[out] out_len      Length of the received reply to request.
 *
 * \return The string corresponding to the specified difficulty. If the input
 * value of difficulty is out of range, "Unknown" string will be returned.
 ****************************************************************************/
char *gj_request(const char **path, uint8_t num_paths, const char **key,
		const char **value, uint8_t num_kv_pairs, uint32_t *out_len);

#endif /*_GAMEJOLT_H_*/

/** \} */

