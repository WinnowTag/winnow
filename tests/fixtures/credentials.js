/* The classifier supports the following roles:
 * 
 *   - item_cache: Allowed to send updates to the classifier's item cache
 *   - classification: Allowed to create and query classification jobs and clues.
 * 
 * In addition the 'classifier' role defines the classifier's own identity which
 * it will use to sign any requests it sends to any other service.
 * 
 */
{
    "item_cache": {
        "access_id": "collector_id",
        "secret_key": "collector_secret" 
    },
    "classification": {
        "access_id": "winnow_id",
        "secret_key": "winnow_secret"
    },
    "classifier": {
        "access_id": "classifier_id",
        "secret_key": "classifier_secret"
    }
}
