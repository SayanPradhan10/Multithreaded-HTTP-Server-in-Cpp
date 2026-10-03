// Multithreaded HTTP Server Dashboard Client
//
// The dashboard periodically requests /api/stats
// to display live server information.
//
// IMPORTANT:
// /api/stats is a monitoring endpoint.
// The backend does NOT count it in total_requests.

/**
 * Fetch server statistics and update dashboard metric cards.
 *
 * This request is only for displaying server metrics.
 * It does not represent a user API request.
 */
async function fetchStats() {
    try {
        const response = await fetch('/api/stats');

        if (!response.ok) {
            throw new Error(
                `HTTP error! status: ${response.status}`
            );
        }

        const data = await response.json();

        // Update server information
        document.getElementById('stat-port').textContent =
            window.location.port || '8080';

        // total_requests comes from the backend.
        // /api/stats polling is excluded by the backend.
        document.getElementById('stat-requests').textContent =
            data.total_requests ?? 0;

        document.getElementById('stat-workers').textContent =
            data.worker_threads ?? 8;

        document.getElementById('stat-active').textContent =
            data.active_workers ?? 0;

        document.getElementById('stat-queue').textContent =
            data.queued_tasks ?? 0;

        document.getElementById('stat-uptime').textContent =
            (data.uptime_seconds ?? 0) + 's';

        // Server is reachable
        const badge =
            document.getElementById('server-status-badge');

        const badgeText =
            document.getElementById('server-status-text');

        badge.classList.remove('offline');
        badgeText.textContent = 'ONLINE';

    } catch (err) {

        // Server is unreachable
        const badge =
            document.getElementById('server-status-badge');

        const badgeText =
            document.getElementById('server-status-text');

        badge.classList.add('offline');
        badgeText.textContent = 'OFFLINE';
    }
}


/**
 * Test a user-selected API endpoint
 * and display its response.
 *
 * These requests ARE counted by the backend
 * when they are API requests other than /api/stats.
 */
async function testEndpoint(endpoint) {

    const urlDisplay =
        document.getElementById('response-url');

    const statusTag =
        document.getElementById('response-status');

    const bodyElem =
        document.getElementById('response-body');

    // Display the endpoint being tested
    urlDisplay.textContent = endpoint;

    statusTag.className = 'status-tag';
    statusTag.textContent = 'Fetching...';

    bodyElem.textContent =
        'Sending request to ' + endpoint + '...';

    try {

        const response =
            await fetch(endpoint);

        const statusText =
            `${response.status} ${
                response.statusText ||
                (
                    response.status === 200
                        ? 'OK'
                        : response.status === 404
                            ? 'Not Found'
                            : ''
                )
            }`;

        statusTag.textContent = statusText;

        statusTag.classList.add(
            `status-${response.status}`
        );

        // Check response type
        const contentType =
            response.headers.get('content-type') || '';

        if (contentType.includes('application/json')) {

            const json =
                await response.json();

            bodyElem.textContent =
                JSON.stringify(json, null, 2);

        } else {

            const text =
                await response.text();

            bodyElem.textContent = text;
        }

        /*
         * Update dashboard statistics after
         * the user finishes an API request.
         *
         * This calls /api/stats, but the backend
         * excludes /api/stats from total_requests.
         */
        await fetchStats();

    } catch (err) {

        statusTag.textContent = 'Error';

        statusTag.classList.add('status-500');

        bodyElem.textContent =
            '// Failed to connect to server: ' +
            err.message;
    }
}


/**
 * Handle the Fibonacci compute endpoint.
 *
 * Example:
 * /api/compute?n=30
 */
function testCompute() {

    const n =
        document
            .getElementById('compute-n-input')
            .value
            .trim();

    testEndpoint(
        `/api/compute?n=${encodeURIComponent(n)}`
    );
}


/**
 * Background auto-refresh.
 *
 * Every 2 seconds:
 *
 *     Dashboard
 *          ↓
 *     GET /api/stats
 *          ↓
 *     Update metrics
 *
 * The backend intentionally does NOT count
 * these monitoring requests as API requests.
 */
setInterval(() => {

    const checkbox =
        document.getElementById('auto-refresh-toggle');

    if (checkbox && checkbox.checked) {
        fetchStats();
    }

}, 2000);


/**
 * Initial dashboard load.
 *
 * This gets the current server statistics once
 * when the page finishes loading.
 */
window.addEventListener('DOMContentLoaded', () => {
    fetchStats();
});