For most people, websites are visual experiences. We see images, we read text, we move the mouse,
scroll with our fingers, tap or click on buttons, etc. The only way most people change how
they interact with sites is by switching to another device, such as from a computer to a
phone.

However, for many people this is not the case. In 2020, there were 7.5 million americans
with visual impairments^[census-disability-2020] with that number rising to over 8 million
in 2021^[census-disability-2021]. For these Americans, "seeing" website images, text, buttons,
etc. is either extremely difficult or not an option. Additionally, other disabilities can impact a
user's ability to interact with many websites, such as paralysis limiting ability to move mouses,
lack of fine motor controls making it hard or impossible to tap or click on desired elements,
cognitive impairment, hearing loss, etc. The CDC states that 61 million Americans live with a
disability^[cdc-disability].

When the internet was first made, it was very accessible^[quarts-a11y]. Most pages were made of text
with most text being fairly well described with components. Interactivity, inaccessible styling,
heavy use of images, etc. was not very common. Adapting a text document to a screen reader was fairly
straight forward and automatic. Today, semantic HTML code is still mostly accessible by default^[mdn-accessibility]^[web-accessible-by-default]. The only exceptions are with
some newer, less used tags that require a bit of extra setup, like <video>^[webaxe-video]. If
developers write good code, then most websites should be accessible without much overhead.

Yet many websites are not accessible, and because of that many developers are pushing the disabled
off the internet or even away from computing entirely. Americans with disabilities are less likely
to own an internet connected device and are three times more likely to not use the internet^[pew-research].
Research in the UK also confirmed that people with disabilities are over twice as likely to not use
the internet^[guardian-research].

This is not to say most developers are malicious or intentionally pushing people off the internet.
However, malice is not needed to create inaccessible websites. Only incompetence, lack of knowledge
or training, lack of testing, or lack of budget and priority is needed to create a website that
fails to accommodate for the needs of millions of people.

Fortunately, the failures of developers is starting to cost companies money. Many companies are
getting sued or have been sued because of inaccessible websites, including Electronic Arts, Target,
Netflix, Nike, Draft Kings, Amazon, Park Entertainment, and Domino's, with many companies losing
court trials or settling out of court^[a11y-lawsuits]. These losses are starting to create awareness
at the business level, which is starting to change the landscape. Business, like Intuit, invest
in training employees about accessibility with Ted Drake^[ted-drake] being a leading advocate.
Changes at the business level are providing knowledge and training needed to improve developer
skills. They are also starting to help prioritize budgets for accessibility and projects to help
test products for accessibility. As the trend grows to being more accessible, the importance of
knowing how to create accessible websites will grow in the web development community.

For information on how to make accessible websites, W3C has published several articles on
accessibility^[w3c-a11y].The a11y project also provides many resources on the topic^[a11-project].

For those who want a similar format to "100 Days of Code", there is "100 days of A11y"^[100-days].

For those wanting more "hands on" guidance, there is the book "Inclusive Components" by Heydon
Pickering^[incl-book], or even his associated blog^[incl-blog].

@References

* census-disability-2020
 | type: web
 | page-title: S1810 Disability Characteristics
 | website-title: United States Census Bureau
 | link: https://data.census.gov/cedsci/table?q=visual%20disability%20in%202020&tid=ACSST5Y2020.S1810
 | date-accessed: 2022-09-28
* census-disability-2021
 | type: web
 | page-title: S1810 Disability Characteristics
 | website-title: United States Census Bureau
 | link: https://data.census.gov/cedsci/table?q=visual%20disability%20in%202021&tid=ACSST1Y2021.S1810
 | date-accessed: 2022-09-28
* cdc-disability
 | type: web
 | page-title: Disability Impacts All of Us
 | website-title: Center for Disease Control and Prevention
 | link: https://www.cdc.gov/ncbddd/disabilityandhealth/infographic-disability-impacts-all.html
 | date-accessed: 2022-09-28
* quarts-a11y
 | type: web
 | page-title: There's already a blueprint for a more accessible internet. If only designers would learn it
 | website-title: Quartz
 | link: https://qz.com/1407450/theres-already-a-blueprint-for-a-more-accessible-internet/
 | date-accessed: 2022-09-28
* mdn-accessibility
 | type: web
 | page-title: HTML: A good basis for accessibility
 | website-title: mdn web docs
 | link: https://developer.mozilla.org/en-US/docs/Learn/Accessibility/HTML
 | date-accessed: 2022-09-28
* web-accessible-by-default
 | type: web
 | page-title: The Web is Accessible by Default, Stop Breaking It
 | website-title: DEV
 | link: https://dev.to/gkemp94/the-web-is-accessible-by-default-stop-breaking-it-4cg4
 | date-accessed: 2022-09-28
* webaxe-video
 | type: web
 | page-title: Accessibility for HTML5 Video, Controls, and Captions
 | website-title: WebAxe
 | link: https://www.webaxe.org/accessibility-for-html5-video-controls-and-captions/
 | date-accessed: 2022-09-28
* pew-research
 | type: web
 | page-title: Americans with disabilities less likely than those without to own some digital devices
 | website-title: Pew Research Center
 | link: https://www.pewresearch.org/fact-tank/2021/09/10/americans-with-disabilities-less-likely-than-those-without-to-own-some-digital-devices/
 | date-accessed: 2022-09-28
* guardian-research
 | type: web
 | page-title: How the internet still fails disabled people
 | website-title: The Guardian
 | link: https://www.theguardian.com/technology/2015/jun/29/disabled-people-internet-extra-costs-commission-scope
 | date-accessed: 2022-09-28
* a11y-lawsuits
 | type: web
 | page-title: Top Companies That Got Sued Over Web Accessibility
 | website-title: Accessi.org
 | link: https://www.accessi.org/blog/famous-web-accessibility-lawsuits/
 | date-accessed: 2022-09-28
* ted-drake
 | type: web
 | page-title: Ted Drake
 | website-title: Intuit Blog
 | link: https://www.intuit.com/blog/author/ted-drake/#:~:text=%EE%80%80Ted%20Drake%EE%80%81%20%EE%80%80Ted%20Drake%EE%80%81%20is%20an%20experienced%20engineer%2C,employees%20and%20promotes%20Intuit%E2%80%99s%20diversity%20in%20hiring%20programs.
 | date-accessed: 2022-10-20
* w3c-a11y
 | type: web
 | page-title: Introduction to Web Accessibility
 | website-title: W3C Web Accessibility Initiative (WAI)
 | link: https://www.w3.org/WAI/fundamentals/accessibility-intro/
 | date-accessed: 2022-10-20
* a11y-project
 | type: web
 | website-title: The a11y project
 | link: https://www.a11yproject.com/
 | date-accessed: 2022-10-20
* 100-days
 | type: web
 | website-title: 100 Days of A11y
 | link: https://100daysofa11y.com/
 | date-accessed: 2022-10-20
* incl-book
 | type: web
 | page-title: Inclusive Components: The Book
 | link: http://book.inclusive-components.design/
 | date-accessed: 2022-10-20
* incl-blog
 | type: web
 | website-title: Inclusive Components
 | link: https://inclusive-components.design/
 | date-accessed: 2022-10-20