/*
 * Contains definitions for the "blackboard"
 */
#ifndef	__BLACKBOARD_HEADER__
#define	__BLACKBOARD_HEADER__

/*
 * Define any handy constants and structures.
 * Also define the functions.
 */
#define	BBOARD_MAX_LINES	4
#define	BBOARD_MAX_LINE_LENGTH	256

#define FULL		0
#define EMPTY		1
#define CRIT		2

struct blackboard {
	/** indicate the number of lines on the blackboard */
	int	nextEmpty;
	int	nextFull;

	/** the contents of the blackboard */
	char	data[BBOARD_MAX_LINES][BBOARD_MAX_LINE_LENGTH];
};

#endif
