#ifndef SCOPE_CAPABILITIES_H
#define SCOPE_CAPABILITIES_H

#define SCOPE_CAP_PRESENT

#define SCOPE_CAP_URL_PLAYER		// Since polyspeculum_1_alpha_1
#define SCOPE_CAP_HTTP_LOGGER		// Since polyspeculum_1_alpha_1
#define SCOPE_CAP_CONSOLE_LOGGER	// Since polyspeculum_1_alpha_1
#define SCOPE_CAP_ECMASCRIPT_LOGGER	// Since polyspeculum_1_alpha_2
#define SCOPE_CAP_OPSOCKET_SECURE_CREATE_ONLY // The 'secure' parameter that were used in a bunch of OpSocket methods are now only used in Create().
#define SCOPE_CAP_CONNECTION_CONTROL 1	// Since polyspeculum_2_beta_3
#define SCOPE_CAP_WINDOW_MANAGER	// Since polyspeculum_3_beta_3
#define SCOPE_CAP_WINDOW_HTTPLOGGER	// Since polyspeculum_3_beta_3
#define SCOPE_CAP_INSPECT_ELEMENT	// Since polyspeculum_4_beta_2
#define SCOPE_CAP_ENABLE_DISABLE	// Since polyspeculum_4_beta_4. NOTE: Feature was available in polyspeculum_4_beta_1.
#define SCOPE_CAP_WINDOW_PAINTED	// Since polyspeculum_5_beta_1
#define SCOPE_CAP_RESPONSE_RECEIVED	// Since polyspeculum_4_beta_6. New callback for url to call when a response is receieved
#define SCOPE_CAP_NOTIFY_WINDOW_LOADED // Whether scope can be notified when a window is loaded.


#endif // SCOPE_CAPABILITIES_H
