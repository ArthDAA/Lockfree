const ACCENTS_MAP = {
	'e': ['é', 'è', 'ê', 'ë'], 'E': ['É', 'È', 'Ê', 'Ë'],
	'a': ['à', 'â', 'æ', 'ä'], 'A': ['À', 'Â', 'Æ', 'Ä'],
	'u': ['ù', 'û', 'ü'], 'U': ['Ù', 'Û', 'Ü'],
	'i': ['î', 'ï'], 'I': ['Î', 'Ï'],
	'o': ['ô', 'œ', 'ö'], 'O': ['Ô', 'Œ', 'Ö'],
	'c': ['ç'], 'C': ['Ç'],
	'y': ['ÿ'], 'Y': ['Ÿ']
};

const ALL_ACCENT_KEYS = Object.keys(ACCENTS_MAP);

let isAltHeld = false;
let popupElement = null;
let badgeElement = null;
let accentState = {
	indices: {},
	lastBaseKey: null,
	activeNode: null
};

function createStyles()
{
	if (document.getElementById('accentflow-styles'))
		return;
	const styles = document.createElement('style');
	styles.id = 'accentflow-styles';
	styles.textContent = `
		.accentflow-highlight {
			background-color: #111827 !important;
			color: white !important;
			border-radius: 3px;
			padding: 0 2px;
		}
		.accentflow-popup {
			position: fixed;
			background: white;
			border: 2px solid #e5e7eb;
			border-radius: 8px;
			padding: 8px;
			box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.1);
			z-index: 2147483647;
			display: none;
			pointer-events: none;
		}
		.accentflow-popup-options {
			display: flex;
			gap: 4px;
		}
		.accentflow-option {
			width: 32px;
			height: 32px;
			display: flex;
			align-items: center;
			justify-content: center;
			font-size: 18px;
			font-weight: bold;
			border-radius: 6px;
			background: #f9fafb;
			color: #374151;
			transition: all 0.15s;
		}
		.accentflow-option.selected {
			background: #3b82f6;
			color: white;
		}
		.accentflow-badge {
			position: fixed;
			top: 20px;
			right: 20px;
			background: #6b7280;
			color: white;
			padding: 6px 12px;
			border-radius: 16px;
			font-size: 12px;
			font-weight: 500;
			z-index: 2147483646;
			display: none;
			box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
		}
		.accentflow-badge.active {
			background: #dc2626;
		}
	`;
	document.head.appendChild(styles);
}

function createPopup()
{
	popupElement = document.createElement('div');
	popupElement.className = 'accentflow-popup';
	popupElement.innerHTML = '<div class="accentflow-popup-options"></div>';
	document.body.appendChild(popupElement);

	badgeElement = document.createElement('div');
	badgeElement.className = 'accentflow-badge';
	badgeElement.textContent = 'Mode Accent';
	document.body.appendChild(badgeElement);
}

function resetAccentState()
{
	const node = accentState.activeNode;
	if (node && node.parentNode && node.parentNode.contains(node))
	{
		const textNode = document.createTextNode(node.textContent);
		node.parentNode.replaceChild(textNode, node);
		
		const selection = window.getSelection();
		const range = document.createRange();
		range.setStartAfter(textNode);
		range.collapse(true);
		selection.removeAllRanges();
		selection.addRange(range);
	}
	
	accentState = {
		indices: {},
		lastBaseKey: null,
		activeNode: null
	};
	
	if (popupElement)
		popupElement.style.display = 'none';
}

function showPopup(baseKey, spanElement)
{
	const variants = ACCENTS_MAP[baseKey];
	const currentIndex = accentState.indices[baseKey] - 1;
	
	const optionsHtml = variants.map((variant, index) => 
		`<div class="accentflow-option ${index === currentIndex ? 'selected' : ''}">${variant}</div>`
	).join('');
	
	popupElement.querySelector('.accentflow-popup-options').innerHTML = optionsHtml;
	
	const rect = spanElement.getBoundingClientRect();
	const popupHeight = 60;
	
	let top = rect.top - popupHeight;
	if (top < 0)
		top = rect.bottom + 10;
	
	popupElement.style.left = `${rect.left}px`;
	popupElement.style.top = `${top}px`;
	popupElement.style.display = 'block';
	
	badgeElement.textContent = 'Sélection';
	badgeElement.classList.add('active');
}

function updatePopupSelection(baseKey)
{
	const currentIndex = accentState.indices[baseKey] - 1;
	const options = popupElement.querySelectorAll('.accentflow-option');
	options.forEach((option, i) => {
		if (i === currentIndex)
			option.classList.add('selected');
		else
			option.classList.remove('selected');
	});
}

function handleKeyDown(event)
{
	if (event.key === 'Alt' && !event.shiftKey && !event.ctrlKey && !event.metaKey)
	{
		isAltHeld = true;
		badgeElement.style.display = 'block';
		badgeElement.textContent = 'Mode Prêt';
		badgeElement.classList.remove('active');
		return;
	}
	
	if (event.altKey && ALL_ACCENT_KEYS.includes(event.key))
	{
		event.preventDefault();
		event.stopPropagation();
		event.stopImmediatePropagation();
		
		const baseKey = event.key;
		const isNewLetter = accentState.lastBaseKey !== baseKey;
		
		if (isNewLetter)
		{
			resetAccentState();
			
			const selection = window.getSelection();
			if (!selection.rangeCount)
				return;
			
			const range = selection.getRangeAt(0);
			const span = document.createElement('span');
			span.className = 'accentflow-highlight';
			
			const currentIndex = 0;
			const newChar = ACCENTS_MAP[baseKey][currentIndex];
			span.textContent = newChar;
			
			range.deleteContents();
			range.insertNode(span);
			
			range.setStartAfter(span);
			range.collapse(true);
			selection.removeAllRanges();
			selection.addRange(range);
			
			accentState.activeNode = span;
			accentState.lastBaseKey = baseKey;
			accentState.indices[baseKey] = 1;
			
			showPopup(baseKey, span);
		}
		else
		{
			const node = accentState.activeNode;
			if (node && node.parentNode && node.parentNode.contains(node))
			{
				const currentIndex = accentState.indices[baseKey];
				const newChar = ACCENTS_MAP[baseKey][currentIndex];
				node.textContent = newChar;
				
				accentState.indices[baseKey] = (currentIndex + 1) % ACCENTS_MAP[baseKey].length;
				
				updatePopupSelection(baseKey);
			}
		}
	}
	else if (!event.altKey)
	{
		resetAccentState();
	}
}

function handleKeyUp(event)
{
	if (event.key === 'Alt')
	{
		isAltHeld = false;
		badgeElement.style.display = 'none';
		resetAccentState();
	}
	else if (!event.altKey)
	{
		isAltHeld = false;
		badgeElement.style.display = 'none';
		resetAccentState();
	}
}

function handleBlur()
{
	isAltHeld = false;
	badgeElement.style.display = 'none';
	resetAccentState();
}

function init()
{
	if (!document.body)
	{
		setTimeout(init, 100);
		return;
	}
	
	createStyles();
	createPopup();
	
	document.addEventListener('keydown', handleKeyDown, true);
	document.addEventListener('keyup', handleKeyUp, true);
	window.addEventListener('blur', handleBlur);
	document.addEventListener('visibilitychange', () => {
		if (document.hidden)
			handleBlur();
	});
}

if (document.readyState === 'loading')
	document.addEventListener('DOMContentLoaded', init);
else
	init();