class CheckedInput:
    def input_number(prompt, min_, max_):
        '''
        Input a number and check if it's in range.
        The input loop will continue as long as the number is not within range, or Ctrl+C
        is pressed. In that case, the function returns None.
        '''
        num = None
        
        while True:
            # Input
            try:
                num = input(prompt)

                # Check if numeric
                num = int(num)
            except KeyboardInterrupt:
                return None
            except:
                print('error: input is not numeric')
                continue

            # Check bounds
            if min_ <= num <= max_:
                break
            else:
                print('error: number should be between {} and {}.'.format(min_, max_))

        return num

    def input_choice(prompt, choices):
        '''
        Input a string that must be equal to one of the strings in the choices list.
        Input loop will continue as long as the input string is not a valid choice, or Ctrl+C
        is pressed. In that case, the function returns None.
        '''
        c = None

        while True:
            try:
                c = input(prompt)
            except KeyboardInterrupt:
                return None

            if c not in choices:
                print('error: options are: {}'.format(choices))
            else: break

        return c
    
    def input_yes_no(prompt):
        '''
        Input a yes or a no answer. Returns True if Yes was input, or False if No was input.
        Input loop will continue as long as the answer is not a yes or a no, or Ctrl+C
        is pressed. In that case, the function returns None.
        '''
        ans = CheckedInput.input_choice(prompt, ['y', 'n', 'Y', 'N'])
        return ans[0].lower()

    def input_choice_numeric_list(prompt, choices, allow_abort=False):
        '''
        Input a string that must be equal to one of the strings in the choices list.
        The choices list will be printed as a numeric list, and the user will have to type
        the number of the choice.
        
        If allow_abort is True, a 0 option to abort input is enabled. If the user chooses
        this option and aborts the input, None is returned.
        
        The input loop will continue as long as the choice number is not valid, or Ctrl+C
        is pressed. In that case, the function returns None.
        '''
        min_number = 1
        
        # Print prompt
        print(prompt)
        
        # Print options
        for i, choice in zip(range(1, len(choices) + 1), choices):
            print('{}) {}'.format(i, choice))
            
        # Print abort option, maybe?
        if allow_abort:
            min_number = 0
            print('0) Abort')
            
        # Now do actual input!
        return CheckedInput.input_number('> ', min_number, len(choices))
