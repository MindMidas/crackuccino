# Crackuccino API

This is the JSON API used by the Crackuccino dashboard.

| Item | Value |
|---|---|
| Local URL | `http://127.0.0.1:8010` |
| Errors | `{"error": "message"}` |
| Session | Browser cookie, handled by the server |

## `GET /api/health`

Checks that the server is running and the native tools are available.

## `POST /api/encrypt`

Encrypts plaintext with a substitution mapping.

```json
{
  "plaintext": "this is a test",
  "encryptionDictionary": "eastih"
}
```

`encryptionDictionary` is optional. If it is included, it must use the same unique letters as the plaintext.

Example response:

```json
{
  "plaintext": "this is a test",
  "inputDictionary": "thisae",
  "encryptionDictionary": "isthae",
  "ciphertext": "example ciphertext"
}
```

## `POST /api/decrypt/direct`

Decrypts with a known mapping. This does not run MPI.

```json
{
  "ciphertext": "eioop",
  "inputDictionary": "hello",
  "encryptionDictionary": "eioop"
}
```

Params:

- `ciphertext`: text to decrypt.
- `encryptionDictionary`: cipher letters. The UI labels this row as the cipher mapping.
- `inputDictionary`: plaintext letters. The UI labels this as the decryption/plain mapping.

The two dictionary strings are paired by position. Extra letters are ignored. Repeated cipher letters are rejected.

Example response:

```json
{
  "ciphertext": "eioop",
  "inputDictionary": "hello",
  "encryptionDictionary": "eioop",
  "plaintext": "hello"
}
```

## `GET /api/dictionaries`

Returns the built-in dictionaries.

```json
{
  "dictionaries": [
    { "id": "brew_dictionary", "label": "Brew", "source": "builtin" }
  ]
}
```

## `GET /api/dictionaries/{id}`

Returns a small preview for one built-in dictionary.

Example response:

```json
{
  "id": "brew_dictionary",
  "label": "Brew",
  "source": "builtin",
  "wordCount": 25,
  "words": ["brew", "cappuccino"],
  "truncated": false
}
```

## `POST /api/dictionaries`

Checks a custom dictionary before the frontend uses it.

```json
{
  "filename": "words.txt",
  "content": "this\nis\ntest\n"
}
```

Rules:

- `.txt` style content
- one word per line
- letters only, with apostrophes allowed inside words
- no permanent server storage

Example response:

```json
{
  "dictionary": {
    "id": "custom:words-abc12345",
    "label": "Words",
    "source": "custom",
    "wordCount": 3,
    "words": ["this", "is", "test"],
    "truncated": false,
    "content": "this\nis\ntest\n"
  }
}
```

## `POST /api/runs`

Starts a serial or MPI search run.

```json
{
  "ciphertext": "example ciphertext",
  "plaintext": "this is a test",
  "dictionary": "brew_dictionary",
  "ranks": 4,
  "depth": null,
  "customDictionary": null
}
```

Params:

- `dictionary`: built-in id, or a `custom:*` id returned by `POST /api/dictionaries`.
- `customDictionary`: required only when `dictionary` is custom.
- `ranks`: use `1` for serial search or `2` to `16` for MPI.
- `depth`: optional search-depth limit.

Returns `202` with a run snapshot. Poll the run with `GET /api/runs/{id}`.

## `GET /api/runs`

Returns recent runs for the current browser session.

```json
{ "runs": [] }
```

## `GET /api/runs/{id}`

Returns one run if it belongs to the current browser session.

Common states:

- `queued`
- `running`
- `complete`
- `failed`
- `canceled`

The response includes request info, worker status, results, summary stats, and any safe error message.

## `POST /api/runs/{id}/cancel`

Cancels a run owned by the current browser session.

Example response: a run snapshot with state `canceled` or another final state if it already finished.

## `DELETE /api/runs/{id}`

Deletes a run from the current browser session history. Active runs are canceled first.

```json
{ "deleted": true }
```

## `POST /api/session/disconnect`

Used by the frontend when the page closes so the server can stop active work for that browser session.

```json
{}
```

Example response:

```json
{ "disconnected": true }
```

## Notes

- Search runs are limited so one browser cannot start unlimited MPI work.
- Custom dictionaries are validated again before a run uses them.
- In production mode, state-changing requests must come from the same origin.
