# Claude agent token

Open the agent panel info popup and choose **Change Claude token...** below account usage to enter or replace the Claude Code OAuth token. When credentials are missing, the panel shows a **Set up Claude** banner. Trying to start an agent opens setup; a submitted chat draft stays intact so you can send it after saving. External start and sync requests wait for credentials and continue after saving; cancelling setup discards those pending requests. The dialog overlays the existing chats.

Run `claude setup-token` in a terminal and paste its token into the masked field. Save trims surrounding whitespace and rejects empty values or embedded whitespace. The token is saved as `CLAUDE_CODE_OAUTH_TOKEN` in the Windows user environment, not in project settings or saved chats. User environment values are stored as plain text by Windows. The field disables copying and cutting the secret.

The current editor uses the replacement immediately for its next agent launch and account usage request. Running agents retain their current credentials until relaunched. Restarting the editor also uses the saved value, even if the parent process has an older environment. When no user value exists, an inherited process token is still supported.
