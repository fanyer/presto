/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef THUMBNAIL_LOGO_CHOOSER_H
#define THUMBNAIL_LOGO_CHOOSER_H

#if defined(CORE_THUMBNAIL_SUPPORT) && defined(THUMBNAILS_LOGO_FINDER)
/**
 * ThumbnailLogoFinder
 *
 * Allows scoring images found on a page to find the one that most probably is the page logo.
 * Uses some heuristics to find the logo (the basic ideas were found in the Mini Server code).
 * Images are scored based on their position, URL, ALT and, if possible, the URL that they are linked to.
 *
 * @note The scoring is fixed by experimenting with some popular sites and the currently used values may
 * not be the best or final values. Though on changing the scoring proceed careful since "fixing" a logo
 * on one site will probably break it on a couple of others.
 *
 * This class only gathers information about the candidates' positions since this is the only
 * thing we need to know to zoom on a chosen logo candidate.
 *
  */
class ThumbnailLogoFinder
{
public:

	/**
	 * Represents a single logo candidate.
	 */
	struct LogoCandidate {
		OpRect	position;
		int		score;
	};

	/**
	 * The constructor uses the specified FramesDocument instance to determine the site name and the site URL.
	 * This information is used while scoring the images.
	 *
	 * @param doc is the FramesDocument instance for which to score candidates.
	 */
	ThumbnailLogoFinder();

	~ThumbnailLogoFinder();

	OP_STATUS Construct(FramesDocument* frm_doc);

	/**
	 * Uses a LogoCandidateChooser to find a best rectangle to zoom on, strongly believing that it contains the page logo.
	 * Will return an empty rect of (0, 0, 0, 0) if an error occurs of if a logo can't be found.
	 *
	 * @param window is the window that contains the document that we scan for the logo.
	 * @param thumbnail_width is the width of the requested thumbnail
	 * @param thumbnail_height is the height of the requested thumbnail
	 * @param logo_rect is set to the actual rect that the logo fits in
	 */
	void FindLogoRect(long thumbnail_width, long thumbnail_height, OpRect* logo_rect);

private:
	/**
	 * Scores an image based on its position, name, ALT and the URL that it links to, if any.
	 * If the image doesn't get enough score to pass the threshold, it will be dropped.
	 *
	 * @param image_path is a part of the URL string that identifies the image location. It contains the image name and the server path to the image, it does not contain the server name of the URL.
	 * @param image_alt is the ALT string for the image, that is searched for the same keywords as the URL string.
	 * @param rect is the position of the image on the page. The proximity to the top-left corner and the size influence the score.
	 *
	 * @returns OpStatus::OK if everything went OK, an error code otherwise.
	 */
	OP_STATUS AddImageCandidate(const OpStringC& image_path, const OpStringC& image_alt, OpRect rect);

	/**
	 * Chooses the best logo candidate from within the ones that scored enough to pass the threshold so far. Basically takes the candidate with the
	 * highest score.
	 * The candidates are owned by the ThumbnailLogoFinder and the memory allocated for them will be freed upon the ThumbnailLogoFinder is destroyed.
	 *
	 * @returns A Candidate pointer if any found or NULL if there is no suitable image for logo (i.e. none of the images fed to AddImageCandidate scored
	 *            high enough to qualify for logo, or none were added at all. The pointer is valid as long as the ThumbnailLogoFinder is alive.
	 */
	LogoCandidate *GetBestCandidate();

	/**
	 * Calculates and returns the score of an image based on the specified position.
	 *
	 * @param rect is the image position
	 *
	 * @returns Calculated score or -1 if the image fails to pass basic size and position checks for a logo.
	 */
	int ScoreImagePos(OpRect rect);

	/**
	 * Scores the image based on its name, ALT and the URL that it links to. See implementation for details.
	 *
	 * @param image_path is a part of the URL string that identifies the image location. It contains the image name and the server path to the image, it does not contain the server name of the URL.
	 * @param image_alt is the ALT string for the image, that is searched for the same keywords as the URL string.
	 * @param image_link_url is the URL that this image is linked to (i.e. an <IMG> inside an <A>), if any. Image will get a bonus score if it links back.
	 *                         to the site URL determined in the constructor.
	 *
	 * @returns Calculated score or -1 if the image fails to pass basic size and position checks for a logo.
	 */
	int ScoreImage(const OpStringC &image_path, const OpStringC &image_alt);

	/**
	 * Checks the provided IMG HTML element for the image attributes and sends the information to the
	 * LogoCandidateChooser that will score it according to the attributes.
	 *
	 * @param window is the window containing the document that contains the HTML element, used to retrieve URLs correctly.
	 * @param element is the IMG HTML element to be inspected for attributes.
	 * @param chooser is a reference to the LogoCandidateChooser that should receive notification about a new logo candidate.
	 *
	 * @returns OpStatus::OK when everything went OK, error code otherwise.
	 */
	OP_STATUS CheckHTMLElementIMG(HTML_Element* element);

	/**
	 * Checks the priovided HTML element for the background CSS image attributes and sends the information to the
	 * LogoCandidateChooser that will score it according to the attributes.
	 *
	 * @param window is the window containing the document that contains the HTML element, used to retrieve URLs correctly.
	 * @param element is the HTML element to be inspected for the BG CSS attributes.
	 * @param chooser is a reference to the LogoCandidateChooser that should receive notification about a new logo candidate.
	 *
	 * @returns OpStatus::OK when everything went OK, error code otherwise.
	 */
	OP_STATUS CheckHTMLElementCSS(HTML_Element* element);

	/**
	 * The site URL as taken from the FramesDocument passed to the constructor, used to compare with URLs that the image candidates link to.
	 */
	URL m_site_url;

	/**
	 * The site name as extracted from the site URL, i.e. "http://www.opera.com/wincesdk" -> "opera".
	 */
	OpString m_site_name;

	/**
	 * Holds and owns the Candidate elements that scored high enough to be a logo candidate.
	 */
	OpVector<LogoCandidate> m_candidates;

	FramesDocument* m_frm_doc;
};
#endif // defined(CORE_THUMBNAIL_SUPPORT) && defined(THUMBNAILS_LOGO_FINDER)
#endif // THUMBNAIL_LOGO_CHOOSER_H
